#!/usr/bin/env python3
"""Rule-based baseline republic: the smallest autonomous controller for OpenRepublicEconomyHost.

It spawns the host as a controller, then repeats: ask `advise`, place a missing raw producer
near its consumer, apply adviser retargets, create newly advised haul orders, step, and poll
every queued command to a terminal result. It is a baseline for LLM or learned controllers,
not a claim about WRSR behaviour.

  python tools/republic_autopilot.py --host <OpenRepublicEconomyHost.exe> [--rounds 5] [--ticks 500]
         [host args after --]  e.g. -- --region <map dir> --content-root <media_soviet> --radius 450
"""
import argparse
import json
import subprocess
import sys


MAX_TICKS = 100_000
MAX_RETAINED_COMMAND_RESULTS = 4096
TRADE_RATE_CAP = 100
TRADE_STOCK_FLOOR = 10


class Host:
    def __init__(self, exe, extra):
        self.proc = subprocess.Popen([exe, "--role", "controller", *extra], stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, text=True, bufsize=1)
        self.next_id = 0
        self.session_id = None
        self.methods = set()
        try:
            hello = self.call("hello")
            self.methods = {entry["name"] for entry in hello.get("methods", [])
                            if isinstance(entry, dict) and isinstance(entry.get("name"), str)}
        except Exception:
            self.proc.terminate()
            self.proc.wait(timeout=10)
            raise

    def call(self, method, **params):
        self.next_id += 1
        request = {"protocolVersion": 0, "id": str(self.next_id), "method": method, **params}
        if self.session_id is not None and method not in ("hello", "capabilities"):
            request["sessionId"] = self.session_id
        self.proc.stdin.write(json.dumps(request) + "\n")
        self.proc.stdin.flush()
        line = self.proc.stdout.readline()
        if not line:
            raise RuntimeError(f"{method} failed: host closed stdout")
        reply = json.loads(line)
        if not reply.get("ok"):
            raise RuntimeError(f"{method} failed: {reply.get('error')}")
        if isinstance(reply.get("sessionId"), str) and reply["sessionId"]:
            self.session_id = reply["sessionId"]
        return reply["result"]

    def close(self):
        try:
            self.call("shutdown")
        except Exception:
            pass
        self.proc.wait(timeout=10)


def _check_error(result, method):
    code = result.get("errorCode")
    if code != "none":
        detail = result.get("message", "")
        raise RuntimeError(f"{method} failed: {code or 'missing errorCode'}{': ' + detail if detail else ''}")


def _world(host):
    result = host.call("world")
    _check_error(result, "world")
    return result["world"]


def _queue_single(host, command, revision=None):
    """Submit one command at the current revision. A non-queued result is a rejection."""
    if revision is None:
        revision = _world(host)["revision"]
    result = host.call("submit", revision=revision, command=command)
    if result.get("errorCode") != "none" or result.get("state") != "queued" or not result.get("handle"):
        return None
    return result["handle"]


def _step(host, ticks):
    result = host.call("step", ticks=ticks)
    _check_error(result, "step")
    return result


def _poll_terminal(host, handle):
    result = host.call("poll", handle=handle)
    _check_error(result, "poll")
    if result.get("state") not in ("accepted", "rejected"):
        raise RuntimeError(f"poll for command {handle} did not reach a terminal state: {result.get('state')}")
    return result["state"]


def _compact_history(host, snapshot, keep_recent):
    """Retire old results only after outcome polls, on hosts advertising the method.

    Explicit maintenance changes the history hash/revision, not production or tick.
    Older hosts remain supported, and 'all' retention disables this maintenance.
    """
    if keep_recent is None or "compact_history" not in getattr(host, "methods", set()):
        return snapshot, 0
    for attempt in range(2):
        history = snapshot.get("commandHistory", {})
        if not isinstance(history, dict):
            raise RuntimeError("world returned invalid command history metadata")
        try:
            if history.get("version") != 1:
                raise ValueError("unsupported history contract")
            retained = int(history["retainedTerminalResults"])
            if retained < 0:
                raise ValueError("negative retained count")
        except (KeyError, TypeError, ValueError) as error:
            raise RuntimeError("world returned invalid command history metadata") from error
        if retained <= keep_recent:
            return snapshot, 0
        result = host.call("compact_history", revision=snapshot["revision"], keepRecent=keep_recent)
        if result.get("errorCode") == "stale_revision" and attempt == 0:
            snapshot = _world(host)
            continue
        _check_error(result, "compact_history")
        try:
            removed = int(result["removedResults"])
            if removed < 0:
                raise ValueError("negative removed count")
        except (KeyError, TypeError, ValueError) as error:
            raise RuntimeError("compact_history returned invalid removed count") from error
        return _world(host), removed
    raise RuntimeError("compact_history did not complete")


def raw_producer_type(catalog, resource):
    """A building type that makes `resource` from nothing (extractor or generator), if any."""
    for building in catalog["buildingTypes"]:
        for recipe in building["recipes"]:
            if not recipe["inputs"] and any(o["resource"] == resource for o in recipe["outputs"]):
                return building["name"]
    return None


def raw_resources(catalog, count_generated=False):
    """Resources made from nothing; optionally count generated power as scored output."""
    consumed = {a["resource"] for b in catalog["buildingTypes"] for r in b["recipes"] for a in r["inputs"]}
    raw = set()
    for building in catalog["buildingTypes"]:
        for recipe in building["recipes"]:
            if not recipe["inputs"]:
                raw.update(o["resource"] for o in recipe["outputs"] if not (
                    count_generated and o["resource"] in consumed and
                    building["name"].startswith("powerplant")))
    return raw


def processed_units(world, raw):
    """Stored amount of every non-raw resource, in whole units (amounts are micro-units)."""
    total = 0
    for building in world["buildings"]:
        for item in building["inventory"]:
            if item["resource"] not in raw:
                total += int(item["amount"])
    return total / 1_000_000


def place_near(host, catalog, type_name, cell):
    """Place a building so its access cell sits on a road, nearest road cell first."""
    width = next(b["width"] for b in catalog["buildingTypes"] if b["name"] == type_name)
    cells = set()
    for road in _world(host)["roads"]:
        if road["enabled"]:
            for end in ("a", "b"):
                cells.add((road[end]["x"], road[end]["z"]))
    candidates = sorted(cells, key=lambda c: abs(c[0] - cell["x"]) + abs(c[1] - cell["z"]))
    for rx, rz in candidates[:400]:
        handle = _queue_single(host, {
            "kind": "place_building", "type": type_name,
            "x": rx - (width - 1) // 2, "z": rz + 1})
        if handle is None:
            continue
        _step(host, 1)
        if _poll_terminal(host, handle) == "accepted":
            return True
    return False


def _retarget_command(suggestion):
    """Keep the wire command additive-protocol compatible and limited to its exact schema."""
    command = suggestion.get("command")
    if not isinstance(command, dict) or command.get("kind") != "retarget_haul_order":
        raise RuntimeError("advise returned an invalid retarget command")
    if "orderId" not in command or "destination" not in command:
        raise RuntimeError("advise returned an incomplete retarget command")
    return {"kind": "retarget_haul_order", "orderId": command["orderId"],
            "destination": command["destination"]}


def _plan(host):
    result = host.call("advise")
    _check_error(result, "advise")
    # Older hosts do not include retargets. Treat the additive field as optional.
    result.setdefault("retargets", [])
    return result


def _new_stats():
    return {"placements_accepted": 0, "retargets_queued": 0, "accepted_retargets": 0,
            "rejected_retargets": 0, "creates_queued": 0, "accepted_creates": 0,
            "rejected_creates": 0}


def _submit_retargets(host, plan, stats, skip_orders=()):
    handles = []
    blocked_pairs = set()
    queued_orders = set()
    current_orders = {order["id"]: order for order in _world(host)["orders"]}
    for suggestion in plan.get("retargets", []):
        command = _retarget_command(suggestion)
        if command["orderId"] in skip_orders:
            continue
        # Adviser commands are tied to its snapshot revision. A newer world revision
        # could make the suggestion itself stale even if that newer revision is valid.
        handle = _queue_single(host, command, revision=plan["revision"])
        order = current_orders.get(command["orderId"])
        if handle is None:
            stats["rejected_retargets"] += 1
        else:
            if order is not None:
                blocked_pairs.add((order["source"], command["destination"], order["resource"]))
            handles.append(handle)
            queued_orders.add(command["orderId"])
            stats["retargets_queued"] += 1
    return handles, blocked_pairs, queued_orders


def _create_commands(plan, blocked_pairs=()):
    commands = []
    seen = set()
    for need in plan["unservedInputs"]:
        if not need["producers"]:
            continue
        advised = need["producers"][0]["command"]
        command = {key: advised[key] for key in ("source", "destination", "resource", "vehicleType")}
        key = (command["source"], command["destination"], command["resource"])
        if key not in seen and key not in blocked_pairs:
            seen.add(key)
            commands.append(command)
    return commands


def _queue_creates(host, commands, stats):
    """Batch new orders while refreshing revision for every batch and retain only real handles."""
    handles = []
    for start in range(0, len(commands), 64):
        revision = _world(host)["revision"]
        result = host.call("create_haul_orders", revision=revision, orders=commands[start:start + 64])
        if result.get("errorCode") != "none":
            stats["rejected_creates"] += len(commands[start:start + 64])
            continue
        for queued in result.get("results", []):
            if (queued.get("errorCode") == "none" and queued.get("state") == "queued" and
                    queued.get("handle")):
                handles.append(queued["handle"])
                stats["creates_queued"] += 1
            else:
                stats["rejected_creates"] += 1
    return handles


def _trade_post_types(catalog):
    return {b["name"] for b in catalog.get("buildingTypes", []) if b.get("tradePost") is True}


def _trade_actions(world, catalog, direction=None):
    """Return bounded export/import requests for declared prices and eligible posts."""
    prices = {r["name"]: r for r in catalog.get("resources", [])
              if isinstance(r, dict) and r.get("tradeDeclared") is True}
    posts = _trade_post_types(catalog)
    if not prices or not posts or "treasury" not in world:
        return []
    treasury = int(world["treasury"])
    types = {b["name"]: b for b in catalog.get("buildingTypes", [])}
    actions = []
    for building in world.get("buildings", []):
        if building.get("type") not in posts:
            continue
        definition = types.get(building["type"], {})
        slots = {s["resource"]: s for s in definition.get("storage", [])}
        inventory = {i["resource"]: int(i["amount"]) for i in building.get("inventory", [])}
        # Export only stock beyond a 10-unit reserve. Import only an actually
        # missing recipe input, and only when the current treasury covers it.
        for resource, price in prices.items():
            stock = inventory.get(resource, 0)
            sell = int(price.get("sellPrice", 0))
            surplus = max(0, stock - TRADE_STOCK_FLOOR)
            if direction in (None, "export") and sell > 0 and surplus:
                amount = min(surplus, TRADE_RATE_CAP)
                actions.append(("trade_export", {"buildingId": int(building["id"]),
                    "resource": resource, "amount": amount, "rateCap": amount}))
        if direction == "export":
            continue
        needed = {i["resource"] for recipe in definition.get("recipes", [])
                  for i in recipe.get("inputs", []) if inventory.get(i["resource"], 0) == 0}
        for resource in sorted(needed & prices.keys()):
            buy = int(prices[resource].get("buyPrice", 0))
            slot = slots.get(resource, {})
            capacity = int(slot.get("capacity", 0))
            free = max(0, capacity - inventory.get(resource, 0))
            amount = min(free, TRADE_RATE_CAP, treasury // buy) if buy > 0 else 0
            if amount:
                actions.append(("trade_import", {"buildingId": int(building["id"]),
                    "resource": resource, "amount": amount, "rateCap": amount}))
                treasury -= amount * buy
    return actions


def _queue_trades(host, world, catalog, direction=None):
    handles = []
    for method, params in _trade_actions(world, catalog, direction):
        result = host.call(method, revision=world["revision"], **params)
        if result.get("errorCode") == "none" and result.get("state") == "queued" and result.get("handle"):
            handles.append(result["handle"])
    return handles


def run_round(host, catalog, ticks, placed, round_number=1, count_generated=False,
              retain_command_results=1024):
    """Run one bounded controller round; exposed for deterministic fake-host tests."""
    stats = _new_stats()
    plan = _plan(host)
    initial_world = _world(host)
    placed_this_round = 0
    for need in plan["unservedInputs"]:
        if need["producers"]:
            continue
        maker = raw_producer_type(catalog, need["resource"])
        if maker and need["resource"] not in placed:
            consumer = next((b for b in initial_world["buildings"] if b["id"] == need["building"]), None)
            if consumer and place_near(host, catalog, maker, consumer["cell"]):
                print(f"  placed a {maker} near building {consumer['id']} for {need['resource']}")
                placed.add(need["resource"])
                placed_this_round += 1
    stats["placements_accepted"] = placed_this_round
    # place_near advances the simulation and changes the advice basis. Never use the old plan.
    if _world(host)["revision"] != initial_world["revision"]:
        plan = _plan(host)

    retarget_handles, blocked_pairs, queued_orders = _submit_retargets(host, plan, stats)
    # A concurrent writer can advance the world after advise. Refresh once and
    # retry advice-derived retargets under the newer adviser revision only.
    if _world(host)["revision"] != plan["revision"]:
        plan = _plan(host)
        refreshed_handles, refreshed_pairs, _ = _submit_retargets(host, plan, stats, queued_orders)
        retarget_handles.extend(refreshed_handles)
        blocked_pairs.update(refreshed_pairs)
    # Retargets are queued before creates and use the adviser revision. Exclude any
    # same-tick create that would duplicate the new source/destination/resource tuple.
    create_handles = _queue_creates(host, _create_commands(plan, blocked_pairs), stats)
    trade_handles = _queue_trades(host, _world(host), catalog)
    _step(host, ticks)
    for handle in retarget_handles:
        if _poll_terminal(host, handle) == "accepted":
            stats["accepted_retargets"] += 1
        else:
            stats["rejected_retargets"] += 1
    for handle in create_handles:
        if _poll_terminal(host, handle) == "accepted":
            stats["accepted_creates"] += 1
        else:
            stats["rejected_creates"] += 1
    for handle in trade_handles:
        _poll_terminal(host, handle)
    stats["trade_queued"] = len(trade_handles)
    # Production may create exportable stock during the requested ticks. Apply
    # exports before planning imports so proceeds can fund them in this round.
    post_trade_handles = _queue_trades(host, _world(host), catalog, "export")
    if post_trade_handles:
        _step(host, 1)
        for handle in post_trade_handles:
            _poll_terminal(host, handle)
    stats["trade_queued"] += len(post_trade_handles)
    import_handles = _queue_trades(host, _world(host), catalog, "import")
    if import_handles:
        _step(host, 1)
        for handle in import_handles:
            _poll_terminal(host, handle)
    stats["trade_queued"] += len(import_handles)

    world, removed = _compact_history(host, _world(host), retain_command_results)
    stats["command_results_removed"] = removed
    running = sum(1 for b in world["buildings"] if b["stallReason"] == "running")
    print(f"round {round_number}: placements accepted {stats['placements_accepted']}; "
          f"haul creates queued/accepted/rejected {stats['creates_queued']}/"
          f"{stats['accepted_creates']}/{stats['rejected_creates']}; retargets queued/accepted/rejected "
          f"{stats['retargets_queued']}/{stats['accepted_retargets']}/{stats['rejected_retargets']}; "
          f"old command results retired {removed}; "
          f"tick {world['tick']}, {running}/{len(world['buildings'])} buildings running, "
          f"{len(world['vehicles'])} trucks, score (stored non-raw units) "
          f"{processed_units(world, raw_resources(catalog, count_generated)):.1f}")
    return {"world": world, **stats}


def positive_rounds(value):
    try:
        parsed = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("rounds must be a positive integer") from error
    if parsed <= 0:
        raise argparse.ArgumentTypeError("rounds must be greater than zero")
    return parsed


def tick_count(value):
    try:
        parsed = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("ticks must be an integer from 1 through 100000") from error
    if parsed < 1 or parsed > MAX_TICKS:
        raise argparse.ArgumentTypeError("ticks must be from 1 through 100000")
    return parsed


def retained_results(value):
    if value == "all":
        return None
    try:
        parsed = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("retention must be 0..4096 or all") from error
    if not 0 <= parsed <= MAX_RETAINED_COMMAND_RESULTS:
        raise argparse.ArgumentTypeError("retention must be 0..4096 or all")
    return parsed


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", required=True)
    parser.add_argument("--rounds", type=positive_rounds, default=5)
    parser.add_argument("--ticks", type=tick_count, default=500)
    parser.add_argument("--retain-command-results", type=retained_results, default=1024,
                        metavar="N|all", help="retain 0..4096 completed results after round outcome polls "
                        "on supported hosts (default 1024); all disables history compaction")
    parser.add_argument("--count-electricity", action="store_true",
                        help="also score stored output of power plants (comparable with REPUBLIC-1's definition)")
    parser.add_argument("--json", action="store_true", help="print one JSON result line (controller vs idle baseline)")
    args, host_args = parser.parse_known_args(argv)
    host_args = [a for a in host_args if a != "--"]

    host = Host(args.host, host_args)
    try:
        catalog = host.call("catalog")
        _check_error(catalog, "catalog")
        placed = set()
        raw = raw_resources(catalog, args.count_electricity)
        world = _world(host)
        command_results_retired = 0
        for round_number in range(1, args.rounds + 1):
            result = run_round(host, catalog, args.ticks, placed, round_number=round_number,
                               count_generated=args.count_electricity,
                               retain_command_results=args.retain_command_results)
            world = result["world"]
            command_results_retired += result["command_results_removed"]
            if round_number == 1:
                blocked = [n for n in _plan(host)["unservedInputs"] if not n["producers"]]
                if blocked:
                    resources = sorted({n["resource"] for n in blocked})
                    print("  cannot be served by any building in this world:", ", ".join(resources))
        for b in world["buildings"]:
            held = ", ".join(f"{i['resource']}={int(i['amount']) / 1_000_000:.1f}"
                              for i in b["inventory"] if int(i["amount"]))
            print(f"  building {b['id']} {b['type']}: {b['stallReason']} [{held}]")
        for order in world["orders"]:
            print(f"  order {order['id']} {order['source']}->{order['destination']} "
                  f"{order['resource']}: {order['state']}")
        final_score = processed_units(world, raw)
        final_tick = int(world["tick"])
        orders = len(world["orders"])
    finally:
        host.close()
    if args.json:
        # Idle baseline: same world and tick count with no commands, so the score is attributable.
        idle = Host(args.host, host_args)
        try:
            idle_world = _world(idle)
            while int(idle_world["tick"]) < final_tick:
                _step(idle, min(MAX_TICKS, final_tick - int(idle_world["tick"])))
                idle_world = _world(idle)
            idle_score = processed_units(idle_world, raw)
        finally:
            idle.close()
        print(json.dumps({"benchmark": "stored_non_raw_units", "tick": final_tick, "controller": final_score,
                          "idle": idle_score, "gain": final_score - idle_score, "orders": orders,
                          "generatorsPlaced": sorted(placed), "commandResultsRetired": command_results_retired,
                          "commandResultsRetention": args.retain_command_results}))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except RuntimeError as error:
        print(f"autopilot failed: {error}", file=sys.stderr)
        sys.exit(1)

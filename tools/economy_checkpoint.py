#!/usr/bin/env python3
"""Export a bounded native economy checkpoint from a live OpenRepublic host.

Examples::

  python tools/economy_checkpoint.py --url http://127.0.0.1:8950 --output republic.checkpoint
  python tools/economy_checkpoint.py --pipe OpenRepublicController --output republic.checkpoint

The host remains authoritative. This tool only calls world/begin_checkpoint/read_checkpoint.
"""
import argparse
import base64
import binascii
import json
import os
import re
import sys
import tempfile
import urllib.error
import urllib.request

MAX_CHECKPOINT_BYTES = 2 * 1024 * 1024 + 128
MAX_CHUNK_BYTES = 16_384
MAX_RESPONSE_BYTES = 16 * 1024 * 1024
PIPE_NAME = re.compile(r"[A-Za-z0-9_.-]{1,64}\Z", re.ASCII)


def _decimal(value, name):
    if not isinstance(value, str) or not value or not value.isascii() or not value.isdecimal():
        raise ValueError("%s must be an unsigned decimal string" % name)
    return int(value)


def _result(client, method, **fields):
    value = client.call(method, **fields)
    if not isinstance(value, dict):
        raise ValueError("%s result must be an object" % method)
    code = value.get("errorCode")
    if code != "none":
        raise RuntimeError("%s failed: %s" % (method, code or "missing errorCode"))
    return value


def export_checkpoint(client, path, overwrite=False):
    """Write one validated checkpoint atomically; client.call returns result objects."""
    world = _result(client, "world").get("world")
    if not isinstance(world, dict):
        raise ValueError("world result is missing its world object")
    expected_revision = _decimal(world.get("revision"), "world revision")

    begin = None
    for attempt in range(2):
        begin = client.call("begin_checkpoint", revision=str(expected_revision))
        if not isinstance(begin, dict):
            raise ValueError("begin_checkpoint result must be an object")
        if begin.get("errorCode") == "stale_revision" and attempt == 0:
            world = _result(client, "world").get("world")
            if not isinstance(world, dict):
                raise ValueError("world result is missing its world object")
            expected_revision = _decimal(world.get("revision"), "world revision")
            continue
        break
    if begin is None or begin.get("errorCode") != "none":
        raise RuntimeError("begin_checkpoint failed: %s %s" % (
            (begin or {}).get("errorCode", "unknown"), (begin or {}).get("message", "")))

    checkpoint_id = begin.get("checkpointId")
    total = begin.get("totalBytes")
    if not isinstance(checkpoint_id, str) or not checkpoint_id:
        raise ValueError("checkpointId must be a non-empty string")
    if type(total) is not int or not 1 <= total <= MAX_CHECKPOINT_BYTES:
        raise ValueError("checkpoint totalBytes is outside 1..%d" % MAX_CHECKPOINT_BYTES)
    revision = _decimal(begin.get("revision"), "checkpoint revision")
    tick = _decimal(begin.get("tick"), "checkpoint tick")
    max_chunk = begin.get("maxChunkBytes")
    if type(max_chunk) is not int or not 1 <= max_chunk <= MAX_CHUNK_BYTES:
        raise ValueError("maxChunkBytes is outside 1..%d" % MAX_CHUNK_BYTES)
    if revision != expected_revision:
        raise ValueError("checkpoint revision differs from requested world revision")

    data = bytearray()
    offset = 0
    while offset < total:
        limit = min(max_chunk, total - offset)
        chunk = _result(client, "read_checkpoint", checkpointId=checkpoint_id,
                        offset=offset, limit=limit)
        if (chunk.get("checkpointId") != checkpoint_id or chunk.get("offset") != offset or
                chunk.get("totalBytes") != total or chunk.get("revision") != str(revision) or
                chunk.get("tick") != str(tick)):
            raise ValueError("checkpoint chunk metadata changed during export")
        next_offset = chunk.get("nextOffset")
        more = chunk.get("hasMore")
        if type(next_offset) is not int or not offset < next_offset <= min(total, offset + limit):
            raise ValueError("checkpoint chunk offset is non-advancing or outside the requested range")
        if type(more) is not bool or more != (next_offset < total):
            raise ValueError("checkpoint chunk hasMore disagrees with nextOffset")
        encoded = chunk.get("dataBase64")
        if not isinstance(encoded, str):
            raise ValueError("checkpoint chunk dataBase64 must be text")
        try:
            raw = base64.b64decode(encoded.encode("ascii"), validate=True)
        except (UnicodeEncodeError, binascii.Error, ValueError) as exc:
            raise ValueError("checkpoint chunk has invalid RFC 4648 base64") from exc
        if len(raw) != next_offset - offset:
            raise ValueError("decoded chunk length does not match its offsets")
        data.extend(raw)
        offset = next_offset
    if len(data) != total:
        raise ValueError("assembled checkpoint length does not match totalBytes")

    destination = os.path.abspath(os.fspath(path))
    parent = os.path.dirname(destination)
    if not os.path.isdir(parent):
        raise FileNotFoundError("output parent directory does not exist")
    fd, temporary = tempfile.mkstemp(prefix=".economy-checkpoint-", suffix=".tmp", dir=parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        if overwrite:
            os.replace(temporary, destination)
        else:
            os.link(temporary, destination)
            os.unlink(temporary)
    except Exception:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise
    return {"checkpointId": checkpoint_id, "revision": revision, "tick": tick,
            "totalBytes": total, "path": destination}


class RpcClient:
    def __init__(self, transport):
        self.transport = transport
        self.request_id = 0
        self.session_id = None
        hello = self.call("hello")
        session = hello.get("sessionId")
        if not isinstance(session, str) or not session:
            raise ValueError("hello did not provide a sessionId")
        self.session_id = session

    def call(self, method, **fields):
        self.request_id += 1
        rid = str(self.request_id)
        request = {"protocolVersion": 0, "id": rid, "method": method, **fields}
        if self.session_id is not None and method not in ("hello", "capabilities"):
            request["sessionId"] = self.session_id
        response = self.transport.exchange(request)
        if not isinstance(response, dict) or response.get("protocolVersion") != 0:
            raise ValueError("invalid protocol response")
        if response.get("id") != rid:
            raise ValueError("response request id mismatch")
        session = response.get("sessionId")
        if not isinstance(session, str) or not session:
            raise ValueError("response missing sessionId")
        if self.session_id is not None and session != self.session_id:
            raise ValueError("host session changed during checkpoint export")
        if response.get("ok") is not True:
            raise RuntimeError("RPC %s failed: %s" % (method, response.get("error")))
        if self.session_id is None:
            self.session_id = session
        result = response.get("result")
        if not isinstance(result, dict):
            raise ValueError("RPC result must be an object")
        return result


class HttpTransport:
    def __init__(self, url, timeout=10):
        if not url.endswith("/rpc"):
            url = url.rstrip("/") + "/rpc"
        self.url, self.timeout = url, timeout

    def exchange(self, request):
        body = json.dumps(request, separators=(",", ":")).encode("utf-8")
        req = urllib.request.Request(self.url, body, {"Content-Type": "application/json"}, method="POST")
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as response:
                raw = response.read(MAX_RESPONSE_BYTES + 1)
        except (urllib.error.URLError, TimeoutError) as exc:
            raise RuntimeError("HTTP economy bridge request failed: %s" % exc) from exc
        if len(raw) > MAX_RESPONSE_BYTES:
            raise ValueError("HTTP response exceeds bounded response size")
        return json.loads(raw.decode("utf-8"))


class PipeTransport:
    def __init__(self, name):
        if not isinstance(name, str) or not PIPE_NAME.fullmatch(name):
            raise ValueError("pipe name must be 1..64 ASCII letters, digits, '.', '_' or '-'")
        if os.name != "nt":
            raise OSError("Windows named pipes are only available on Windows")
        self.pipe = open("\\\\.\\pipe\\" + name, "r+b", buffering=0)

    def exchange(self, request):
        self.pipe.write(json.dumps(request, separators=(",", ":")).encode("utf-8") + b"\n")
        line = self.pipe.readline(MAX_RESPONSE_BYTES + 1)
        if not line or len(line) > MAX_RESPONSE_BYTES or not line.endswith(b"\n"):
            raise ValueError("named-pipe response is empty, oversized, or unterminated")
        return json.loads(line[:-1].decode("utf-8"))

    def close(self):
        self.pipe.close()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--url", help="HTTP bridge base URL or /rpc URL")
    group.add_argument("--pipe", help="Windows economy named-pipe name (not a filesystem path)")
    parser.add_argument("--output", required=True, help="checkpoint output file")
    parser.add_argument("--force", action="store_true", help="atomically replace an existing output")
    args = parser.parse_args(argv)
    transport = HttpTransport(args.url) if args.url else PipeTransport(args.pipe)
    try:
        result = export_checkpoint(RpcClient(transport), args.output, overwrite=args.force)
    finally:
        close = getattr(transport, "close", None)
        if close:
            close()
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print("economy checkpoint export failed: %s" % exc, file=sys.stderr)
        sys.exit(1)

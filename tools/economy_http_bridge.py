#!/usr/bin/env python3
"""Loopback HTTP wrapper around OpenRepublicEconomyHost so agents keep one world.

The host speaks one JSON request per stdin line and keeps its world in memory,
so each agent call must reach the same process. This script owns that process
and forwards requests:

    python tools/economy_http_bridge.py --host-exe <OpenRepublicEconomyHost.exe> \
        [--port 8950] [-- <host arguments such as --role controller --region ...>]

    POST /rpc      body = one request object (protocolVersion/id are filled in if
                   missing); response = the host's response object
    GET  /health   {"ok": true, "requests": N}

Trusted-local only: it binds 127.0.0.1, has no authentication and serializes all
calls. Roles come from the host arguments, not from the HTTP client.
"""

import argparse
import itertools
import json
import subprocess
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

MAX_BODY = 60 * 1024


class HostProcess:
    def __init__(self, command):
        self._proc = subprocess.Popen(
            command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, encoding="utf-8", bufsize=1)
        self._lock = threading.Lock()
        self._ids = itertools.count(1)
        self.requests = 0

    def call(self, request):
        if not isinstance(request, dict):
            raise ValueError("request must be a JSON object")
        request = dict(request)
        request.setdefault("protocolVersion", 0)
        request.setdefault("id", "r%d" % next(self._ids))
        line = json.dumps(request, separators=(",", ":"))
        if "\n" in line:
            raise ValueError("request must serialize to one line")
        with self._lock:
            if self._proc.poll() is not None:
                raise RuntimeError("host process has exited (code %s)" % self._proc.returncode)
            self._proc.stdin.write(line + "\n")
            self._proc.stdin.flush()
            response = self._proc.stdout.readline()
            self.requests += 1
        if not response:
            raise RuntimeError("host closed its output")
        return json.loads(response)

    def close(self):
        if self._proc.poll() is None:
            try:
                self._proc.stdin.close()
                self._proc.wait(timeout=5)
            except Exception:
                self._proc.kill()


def make_handler(host):
    class Handler(BaseHTTPRequestHandler):
        def _send(self, status, payload):
            body = json.dumps(payload).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            if self.path == "/health":
                self._send(200, {"ok": True, "requests": host.requests})
            else:
                self._send(404, {"ok": False, "error": "unknown path"})

        def do_POST(self):
            if self.path != "/rpc":
                self._send(404, {"ok": False, "error": "unknown path"})
                return
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0 or length > MAX_BODY:
                self._send(400, {"ok": False, "error": "body must be 1..%d bytes" % MAX_BODY})
                return
            try:
                request = json.loads(self.rfile.read(length).decode("utf-8"))
                self._send(200, host.call(request))
            except (ValueError, RuntimeError) as exc:
                self._send(400, {"ok": False, "error": str(exc)})

        def log_message(self, fmt, *args):  # keep stdout quiet
            pass

    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("--host-exe", required=True)
    parser.add_argument("--port", type=int, default=8950)
    parser.add_argument("host_args", nargs="*", help="arguments for the host (put after --)")
    args = parser.parse_args()
    host = HostProcess([args.host_exe] + args.host_args)
    server = ThreadingHTTPServer(("127.0.0.1", args.port), make_handler(host))
    print("economy bridge listening on http://127.0.0.1:%d" % args.port, flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        host.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())

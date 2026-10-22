import base64
import os
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

from tools.economy_checkpoint import (
    MAX_CHUNK_BYTES,
    PipeTransport,
    RpcClient,
    export_checkpoint,
)


class FakeClient:
    def __init__(self, data, fault=None, stale=False):
        self.data = data
        self.fault = fault
        self.stale = stale
        self.calls = []
        self.begin_count = 0
        self.world_count = 0

    def call(self, method, **fields):
        self.calls.append((method, fields))
        if method == "world":
            self.world_count += 1
            revision = "7" if self.stale and self.world_count == 1 else "8"
            return {"errorCode": "none", "world": {"revision": revision}}
        if method == "begin_checkpoint":
            self.begin_count += 1
            if self.stale and self.begin_count == 1:
                return {"errorCode": "stale_revision"}
            return {"errorCode": "none", "checkpointId": "cp-1", "totalBytes": len(self.data),
                    "revision": fields["revision"], "tick": "33", "maxChunkBytes": 7}
        if method == "read_checkpoint":
            offset, limit = fields["offset"], fields["limit"]
            raw = self.data[offset:offset + limit]
            result = {"errorCode": "none", "checkpointId": "cp-1", "offset": offset,
                      "nextOffset": offset + len(raw), "totalBytes": len(self.data),
                      "hasMore": offset + len(raw) < len(self.data),
                      "dataBase64": base64.b64encode(raw).decode("ascii"),
                      "revision": "8", "tick": "33"}
            if self.fault == "wrong_offset":
                result["offset"] += 1
            elif self.fault == "no_advance":
                result["nextOffset"] = offset
                result["dataBase64"] = ""
                result["hasMore"] = True
            elif self.fault == "bad_base64":
                result["dataBase64"] = "not base64!"
            elif self.fault == "wrong_size":
                result["totalBytes"] += 1
            elif self.fault == "metadata":
                result["tick"] = "34"
            elif self.fault == "short_chunk":
                result["nextOffset"] = offset + max(1, len(raw) - 1)
            return result
        raise AssertionError(method)


class FakeTransport:
    def __init__(self, mutate=None):
        self.calls = []
        self.session = "session-a"
        self.mutate = mutate

    def exchange(self, request):
        self.calls.append(request)
        response = {"protocolVersion": 0, "id": request["id"], "ok": True,
                    "sessionId": self.session, "result": {"sessionId": self.session}}
        if request["method"] == "hello":
            response["result"].update({"role": "observer"})
        if self.mutate:
            return self.mutate(response, request)
        return response


class CheckpointTests(unittest.TestCase):
    def test_exports_arbitrary_binary_across_chunk_boundaries(self):
        original = bytes(range(256)) * 3 + "é雪".encode("utf-8")
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "state.bin")
            result = export_checkpoint(FakeClient(original), path)
            with open(path, "rb") as stream:
                self.assertEqual(stream.read(), original)
            self.assertEqual(result["totalBytes"], len(original))

    def test_retries_stale_revision_once(self):
        with tempfile.TemporaryDirectory() as folder:
            client = FakeClient(b"payload", stale=True)
            export_checkpoint(client, os.path.join(folder, "state"))
            begins = [fields for method, fields in client.calls if method == "begin_checkpoint"]
            self.assertEqual([item["revision"] for item in begins], ["7", "8"])
            self.assertEqual(client.begin_count, 2)

    def test_rejects_malformed_chunk_and_leaves_no_output_or_temp(self):
        for fault in ("wrong_offset", "no_advance", "bad_base64", "wrong_size", "metadata", "short_chunk"):
            with self.subTest(fault=fault), tempfile.TemporaryDirectory() as folder:
                path = os.path.join(folder, "state")
                with self.assertRaises((ValueError, RuntimeError)):
                    export_checkpoint(FakeClient(b"long enough", fault=fault), path)
                self.assertFalse(os.path.exists(path))
                self.assertEqual(os.listdir(folder), [])

    def test_default_refuses_overwrite_and_force_replaces_atomically(self):
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "state")
            with open(path, "wb") as stream:
                stream.write(b"old")
            with self.assertRaises(FileExistsError):
                export_checkpoint(FakeClient(b"new state"), path)
            with open(path, "rb") as stream:
                self.assertEqual(stream.read(), b"old")
            export_checkpoint(FakeClient(b"new state"), path, overwrite=True)
            with open(path, "rb") as stream:
                self.assertEqual(stream.read(), b"new state")
            self.assertEqual(os.listdir(folder), ["state"])

    def test_missing_output_parent_is_not_created(self):
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "missing", "state")
            with self.assertRaises(FileNotFoundError):
                export_checkpoint(FakeClient(b"x"), path)
            self.assertFalse(os.path.exists(os.path.dirname(path)))

    def test_rpc_client_binds_session_and_checks_request_identity(self):
        transport = FakeTransport()
        client = RpcClient(transport)
        client.call("world")
        self.assertEqual(transport.calls[1]["sessionId"], "session-a")
        self.assertEqual([call["id"] for call in transport.calls], ["1", "2"])

        def wrong_session(response, request):
            response["sessionId"] = "replaced"
            return response
        with self.assertRaisesRegex(ValueError, "session changed"):
            RpcClient(FakeTransport(wrong_session)).call("world")

        def wrong_id(response, request):
            response["id"] = "wrong"
            return response
        transport = FakeTransport()
        client = RpcClient(transport)
        transport.mutate = wrong_id
        with self.assertRaisesRegex(ValueError, "request id"):
            client.call("world")

    def test_named_pipe_name_is_strictly_not_a_path(self):
        for name in ("", "a" * 65, "../escape", "\\\\.\\pipe\\x", "has space"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                PipeTransport(name)

    def test_chunk_limit_is_protocol_bounded(self):
        self.assertEqual(MAX_CHUNK_BYTES, 16_384)


if __name__ == "__main__":
    unittest.main()

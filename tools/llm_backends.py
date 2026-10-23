#!/usr/bin/env python3
"""Small, bounded model backends for llm_republic.py. No backend receives shell tools."""
import json
import os
import subprocess
import tempfile
import urllib.error
import urllib.request


class BackendError(RuntimeError):
    pass


class ScriptedBackend:
    def __init__(self, responses):
        self.responses = list(responses)
        self.index = 0

    def complete(self, prompt, timeout=60):
        if self.index >= len(self.responses):
            return {"actions": [], "notes": "script complete"}
        result = self.responses[self.index]
        self.index += 1
        return result if isinstance(result, dict) else json.loads(result)


def _find_codex():
    """PATH first, then OPENREPUBLIC_CODEX, then the Windows desktop-app install location."""
    import glob
    import os
    import shutil
    found = shutil.which("codex") or os.environ.get("OPENREPUBLIC_CODEX")
    if found:
        return found
    candidates = sorted(glob.glob(os.path.join(os.environ.get("LOCALAPPDATA", ""), "OpenAI", "Codex", "bin", "*", "codex.exe")))
    return candidates[-1] if candidates else "codex"


class CodexBackend:
    def __init__(self, model="gpt-5.6-luna"):
        self.model = model

    def complete(self, prompt, timeout=120):
        # Codex CLI is deliberately run outside the repository with read-only sandboxing.
        with tempfile.TemporaryDirectory(prefix="openrepublic-codex-") as scratch:
            cmd = [_find_codex(), "exec", "-s", "read-only", "-m", self.model,
                   "--skip-git-repo-check", "--ephemeral", "-"]
            try:
                result = subprocess.run(cmd, input=prompt, text=True, capture_output=True,
                                        cwd=scratch, timeout=timeout, check=False)
            except (OSError, subprocess.TimeoutExpired) as error:
                raise BackendError(f"codex backend unavailable or timed out: {error}") from error
        if result.returncode:
            raise BackendError(f"codex exited {result.returncode}: {result.stderr[-1200:]}")
        # `codex exec` emits an agent transcript; the final assistant message is last.
        lines = [line for line in result.stdout.splitlines() if line.strip()]
        for line in reversed(lines):
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                continue
            if event.get("type") == "item.completed":
                item = event.get("item", {})
                if item.get("type") in ("agent_message", "assistant_message"):
                    return parse_json_object(item.get("text", ""))
        # Some CLI versions print the final response as plain text.
        return parse_json_object(result.stdout)


class OpenAICompatibleBackend:
    def __init__(self, base_url=None, model=None, api_key=None):
        self.base_url = (base_url or os.getenv("OPENAI_COMPATIBLE_BASE_URL", "http://127.0.0.1:1234/v1")).rstrip("/")
        self.model = model or os.getenv("OPENAI_COMPATIBLE_MODEL", "local-model")
        self.api_key = api_key if api_key is not None else os.getenv("OPENAI_COMPATIBLE_API_KEY", "")

    def complete(self, prompt, timeout=90):
        body = json.dumps({"model": self.model, "messages": [{"role": "user", "content": prompt}],
                           "temperature": 0, "max_tokens": 1200,
                           "response_format": {"type": "json_object"}}).encode()
        headers = {"Content-Type": "application/json"}
        if self.api_key:
            headers["Authorization"] = "Bearer " + self.api_key
        request = urllib.request.Request(self.base_url + "/chat/completions", body, headers)
        try:
            with urllib.request.urlopen(request, timeout=timeout) as response:
                payload = json.loads(response.read(2_000_001))
            return parse_json_object(payload["choices"][0]["message"]["content"])
        except (OSError, urllib.error.URLError, KeyError, IndexError, TypeError, json.JSONDecodeError) as error:
            raise BackendError(f"openai-compatible request failed: {error}") from error


class ClaudeBackend:
    def complete(self, prompt, timeout=120):
        try:
            result = subprocess.run(["claude", "-p", "--output-format", "json"], input=prompt,
                                    text=True, capture_output=True, timeout=timeout, check=False)
        except (OSError, subprocess.TimeoutExpired) as error:
            raise BackendError(f"claude backend unavailable or timed out: {error}") from error
        if result.returncode:
            raise BackendError(f"claude exited {result.returncode}: {result.stderr[-1200:]}")
        try:
            wrapper = json.loads(result.stdout)
            return parse_json_object(wrapper.get("result", result.stdout))
        except json.JSONDecodeError:
            return parse_json_object(result.stdout)


def parse_json_object(value):
    if isinstance(value, dict):
        return value
    if not isinstance(value, str) or len(value) > 100_000:
        raise BackendError("model response is not a bounded JSON string/object")
    text = value.strip()
    if text.startswith("```"):
        text = text.split("\n", 1)[-1].rsplit("```", 1)[0].strip()
    try:
        parsed = json.loads(text)
    except json.JSONDecodeError as error:
        raise BackendError(f"invalid model JSON: {error}") from error
    if not isinstance(parsed, dict):
        raise BackendError("model JSON root must be an object")
    return parsed


def make_backend(name):
    if name == "codex":
        return CodexBackend()
    if name == "openai-compatible":
        return OpenAICompatibleBackend()
    if name == "claude":
        return ClaudeBackend()
    raise ValueError(f"unknown backend: {name}")

#!/usr/bin/env python3
"""`batbox providers` end to end in a throwaway config directory.

add writes the same endpoint record /providers saves: kind, preset, quirks,
limits from the catalog, the key as a variable name or (from stdin) a private
token, and the selected profile. Unsupported and sign-in-only providers are
refused with a reason; list, show and doctor report what was saved.
"""
import json
import os
import subprocess
import sys
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


def check(condition, message):
    if not condition:
        raise SystemExit("FAIL: " + message)


def main(binary):
    seen = []

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_GET(self):
            seen.append((self.path, self.headers.get("Authorization")))
            if self.path != "/v1/models":
                self.send_error(404)
                return
            if self.headers.get("Authorization") == "Bearer gsk-wrong":
                body = json.dumps({"error": {"message": "Invalid API Key",
                                             "type": "invalid_request_error"}}).encode()
                self.send_response(401)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
                return
            body = json.dumps({"data": [{"id": "cli-coder"}, {"id": "cli-tiny"}]}).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    url = f"http://127.0.0.1:{server.server_port}"
    try:
        with tempfile.TemporaryDirectory(prefix="batbox-providers-cli-") as temporary:
            root = Path(temporary)
            config = root / "config"
            env = {k: v for k, v in os.environ.items()
                   if not k.startswith("BATBOX_") and not k.endswith("_API_KEY")}
            env.update(BATBOX_CONFIG_DIR=str(config), HOME=str(root),
                       NO_PROXY="127.0.0.1,localhost", no_proxy="127.0.0.1,localhost")

            def run(*arguments, stdin=None, extra=None):
                return subprocess.run([str(binary), "providers", *arguments], input=stdin,
                                      env={**env, **(extra or {})}, capture_output=True,
                                      text=True, timeout=60)

            def document():
                return json.loads((config / "native-client.json").read_text())

            local = run("add", "llama.cpp", "--base-url", url)
            check(local.returncode == 0, "add llama.cpp: " + local.stdout + local.stderr)
            check("Connection: Connected to 127.0.0.1" in local.stdout, local.stdout)
            check("Added llama.cpp · cli-coder · /model to switch" in local.stdout, local.stdout)
            saved = document()
            row = saved["endpoints"]["llama.cpp"]
            check(saved["profile"] == "llama.cpp::cli-coder", "profile " + saved["profile"])
            check(row["base_url"] == url and row["messages_path"] == "/v1/messages" and
                  row["auth_header"] == "none" and "provider" not in row and
                  row["preset"] == "llama.cpp" and row["models"] == ["cli-coder", "cli-tiny"],
                  "llama.cpp row " + json.dumps(row))
            check(row["context_window"] == 131072 and row["max_output_tokens"] == 16384,
                  "default limits for an unknown local model")
            check((config / "native-client.json").stat().st_mode & 0o777 == 0o600, "mode 0600")

            again = run("add", "llama.cpp", "--base-url", url, "--model", "cli-tiny")
            check(again.returncode == 0 and "Added llama.cpp-2 · cli-tiny" in again.stdout,
                  "second endpoint id: " + again.stdout + again.stderr)
            check(document()["profile"] == "llama.cpp-2::cli-tiny", "second selection")

            keyed = run("add", "anthropic", "--key", "-", "--model", "claude-sonnet-5-5",
                        "--no-test", stdin="sk-cli-secret\n")
            check(keyed.returncode == 0, "add anthropic: " + keyed.stderr)
            check("sk-cli-secret" not in keyed.stdout + keyed.stderr, "the key was echoed")
            row = document()["endpoints"]["anthropic"]
            check(row["auth_token"] == "sk-cli-secret" and "api_key_env" not in row and
                  row["auth_header"] == "x-api-key" and row["context_window"] == 1000000 and
                  row["max_output_tokens"] == 128000 and row["thinking"] and
                  row["prompt_caching"], "anthropic row " + json.dumps(row))

            routed = run("add", "openrouter", "--key-env", "ROUTER_KEY", "--model",
                         "qwen/qwen3-coder", "--no-test")
            check(routed.returncode == 0, "add openrouter: " + routed.stderr)
            row = document()["endpoints"]["openrouter"]
            check(row["api_key_env"] == "ROUTER_KEY" and "auth_token" not in row and
                  row["base_url"] == "https://openrouter.ai" and
                  row["messages_path"] == "/api/v1/messages", "openrouter row " + json.dumps(row))

            chat = run("add", "groq", "--no-test", "--model", "custom-model",
                       extra={"GROQ_API_KEY": "gsk-from-env"})
            check(chat.returncode == 0, "add groq: " + chat.stderr)
            row = document()["endpoints"]["groq"]
            check(row["provider"] == "openai-chat" and row["api_key_env"] == "GROQ_API_KEY" and
                  row["messages_path"] == "/v1/chat/completions" and
                  row["quirks"]["max_tokens_field"] == "max_tokens", "groq row " + json.dumps(row))
            check("gsk-from-env" not in json.dumps(document()), "an environment key was stored")

            # The connection test lists models through the openai-chat client
            # with the bearer key, and names a rejected key without retrying.
            tested = run("add", "groq", "--base-url", url, "--id", "groq-local",
                         extra={"GROQ_API_KEY": "gsk-good"})
            check(tested.returncode == 0 and
                  "Connection: Connected to 127.0.0.1" in tested.stdout and
                  "· 2 models" in tested.stdout, "groq test: " + tested.stdout + tested.stderr)
            check(("/v1/models", "Bearer gsk-good") in seen, "openai-chat listing sent the key")
            check(document()["endpoints"]["groq-local"]["models"][:2] == ["cli-coder", "cli-tiny"],
                  "live models saved")
            rejected = run("add", "groq", "--base-url", url, "--id", "groq-bad",
                           "--model", "cli-coder", extra={"GROQ_API_KEY": "gsk-wrong"})
            check(rejected.returncode == 0 and "Connection check: The server rejected the key "
                  "(HTTP 401)" in rejected.stdout, "401 wording: " + rejected.stdout)
            check(seen.count(("/v1/models", "Bearer gsk-wrong")) == 1, "a 401 must not retry")

            for refused, needle in (("perplexity", "not supported yet"),
                                    ("github-copilot", "coming soon"),
                                    ("no-such-provider", "no provider")):
                result = run("add", refused, "--no-test")
                check(result.returncode == 2 and needle in result.stderr,
                      f"{refused}: {result.returncode} {result.stderr}")
            bad = run("add", "anthropic", "--key", "sk-inline")
            check(bad.returncode == 2 and "stdin" in bad.stderr, "inline keys are refused")

            listing = run()
            check(listing.returncode == 0, "list failed: " + listing.stderr)
            for needle in ("Configured endpoints", "llama.cpp-2", "Cloud", "opencode",
                           "Local servers", "Unsupported yet", "providers from models.dev"):
                check(needle in listing.stdout, "list lacks " + needle)
            shown = run("show", "zen")
            check(shown.returncode == 0 and "/anthropic/v1/messages" in shown.stdout and
                  "OPENCODE_API_KEY" in shown.stdout, "show zen: " + shown.stdout)
            doctor = subprocess.run([str(binary), "doctor"], env=env, capture_output=True,
                                    text=True, timeout=60)
            check("Model servers on this machine" in doctor.stdout and
                  "Model endpoints" in doctor.stdout and "groq" in doctor.stdout,
                  "doctor: " + doctor.stdout)
            rows = {line.split()[0]: line for line in doctor.stdout.splitlines()
                    if line.startswith("  ") and line.split()}
            check("· openai-chat ·" in rows.get("groq", "") and
                  "· anthropic ·" in rows.get("llama.cpp", ""),
                  "doctor rows name the provider kind: " + doctor.stdout)
            check(("/v1/models", None) in seen, "the connection test listed models")
    finally:
        server.shutdown()
    print("batbox providers: add, refusals, list, show and doctor rows")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

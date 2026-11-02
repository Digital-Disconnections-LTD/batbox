#!/usr/bin/env python3
"""Check gadgets/providers_catalog.py and the committed catalog it produced.

1. The generator is deterministic: two runs over the sample give identical bytes.
2. Kinds, trimming and filters follow the rules for the sample providers.
3. The committed src/providers/Catalog.generated.cpp parses and carries the
   providers the /providers dialog promises.
"""
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

REQUIRED = ["opencode", "llama.cpp", "ollama", "lmstudio", "vllm", "openrouter", "anthropic",
            "openai", "groq", "deepseek", "mistral", "xai", "togetherai", "fireworks-ai",
            "cerebras", "moonshotai", "zai", "minimax", "alibaba", "github-copilot",
            "amazon-bedrock", "google-vertex"]
KINDS = {"anthropic", "openai-chat", "bedrock", "vertex", "unsupported"}


def check(condition, message):
    if not condition:
        raise SystemExit("FAIL: " + message)


def catalog_from_cpp(text):
    chunks = re.findall(r'R"BBX\((.*?)\)BBX"', text, re.S)
    check(chunks, "no catalog chunks in generated file")
    for chunk in chunks:
        check(len(chunk.encode("utf-8")) <= 8000, "a chunk exceeds 8000 bytes")
    joined = "".join(chunks)
    declared = int(re.search(r"catalogBytes\(\)\{return (\d+);\}", text).group(1))
    check(declared == len(joined.encode("utf-8")), "catalogBytes does not match the chunks")
    return json.loads(joined)


def main():
    root = Path(sys.argv[1])
    generator = root / "gadgets/providers_catalog.py"
    sample = root / "tests/phase6/fixtures/models-dev-sample.json"
    with tempfile.TemporaryDirectory() as scratch:
        outputs = []
        for name in ("first.cpp", "second.cpp"):
            path = Path(scratch) / name
            subprocess.run([sys.executable, "-I", str(generator), str(sample), "--output",
                            str(path)], check=True, capture_output=True)
            outputs.append(path.read_bytes())
        check(outputs[0] == outputs[1], "generator output differs between runs")
    document = catalog_from_cpp(outputs[0].decode("utf-8"))
    rows = {row["id"]: row for row in document["providers"]}
    check(rows["anthropic"]["kind"] == "anthropic" and
          rows["anthropic"]["auth_header"] == "x-api-key", "anthropic preset")
    check(rows["opencode"]["messages_path"] == "/anthropic/v1/messages", "Zen messages path")
    check(not any(m[0].endswith("-free") for m in rows["opencode"]["models"]),
          "OpenCode free models leaked into the catalog")
    check(rows["openrouter"]["messages_path"] == "/api/v1/messages", "OpenRouter path")
    models = rows["openrouter"]["models"]
    check(len(models) == 40 and all("t" in m[4] for m in models),
          "OpenRouter list should be trimmed to 40 tool-capable models")
    check(rows["groq"]["kind"] == "openai-chat" and
          rows["groq"]["messages_path"] == "/v1/chat/completions", "groq chat path")
    check(rows["302ai"]["kind"] == "openai-chat" and rows["302ai"]["base_url"] == "https://api.302.ai",
          "openai-compatible split of /v1")
    check(rows["mistral"]["quirks"]["tool_ids"] == "short", "mistral quirks")
    check(rows["perplexity"]["kind"] == "unsupported" and rows["perplexity"]["reason"],
          "unsupported rows carry a reason")
    check(rows["vivgrid"]["kind"] == "unsupported" and "Responses" in rows["vivgrid"]["reason"],
          "Responses-only gateway")
    check(rows["amazon-bedrock"]["kind"] == "bedrock", "bedrock kind")
    check(rows["databricks"].get("base_url_needs") == ["DATABRICKS_HOST"], "URL placeholders")
    check(rows["lmstudio"].get("local") and rows["lmstudio"]["auth"] == "none", "LM Studio local")
    check(rows["freemodel"]["kind"] == "anthropic" and rows["freemodel"]["base_url"] ==
          "https://cc.freemodel.dev", "Anthropic SDK rows become presets")
    for local in ("ollama", "llama.cpp", "vllm"):
        check(rows[local]["kind"] == "anthropic" and rows[local]["local"], local + " entry")
    check([row["id"] for row in document["providers"]][:3] == ["opencode", "anthropic", "openrouter"],
          "priority order")

    committed = catalog_from_cpp((root / "src/providers/Catalog.generated.cpp").read_text("utf-8"))
    ids = {row["id"] for row in committed["providers"]}
    missing = [pid for pid in REQUIRED if pid not in ids]
    check(not missing, "committed catalog lacks " + ", ".join(missing))
    check(len(committed["providers"]) >= 235, "committed catalog lost providers")
    for row in committed["providers"]:
        check(row["kind"] in KINDS, row["id"] + " has kind " + row["kind"])
        check(len(row["models"]) <= 40, row["id"] + " has more than 40 models")
        if row["kind"] == "unsupported":
            check(bool(row.get("reason")), row["id"] + " lacks an unsupported reason")
        else:
            check(row["messages_path"].startswith("/"), row["id"] + " messages path")
    print("provider catalog generator is deterministic; committed catalog has",
          len(committed["providers"]), "providers")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Build Batbox's bundled provider catalog from a models.dev api.json.

Usage:
  python3 gadgets/providers_catalog.py SOURCE [--output src/providers/Catalog.generated.cpp]

SOURCE is a path to api.json or an https:// URL (https://models.dev/api.json).
The output is a C++ file holding a compact JSON document split into string
literals, read by src/providers/Catalog.cpp. The same input always produces
the same bytes, so the generated file can be committed and diffed.

Each provider's wire kind comes from what is known about its endpoints:
servers with a verified Anthropic Messages route become "anthropic" presets,
OpenAI Chat Completions servers become "openai-chat", Bedrock and Vertex keep
their own kinds, and anything else is "unsupported" with a reason the
/providers dialog shows.
"""
import argparse
import json
import sys
import urllib.request
from pathlib import Path

MODEL_LIMIT = 40
CHUNK = 8000
DEFAULT_CONTEXT = 131072
DEFAULT_OUTPUT = 16384

# Providers whose whole service answers Anthropic Messages. base_url is the
# origin plus any shared prefix; messages_path and models_path hang off it.
ANTHROPIC = {
    "anthropic": dict(base="https://api.anthropic.com", path="/v1/messages",
                      auth="x-api-key"),
    "opencode": dict(base="https://opencode.ai/inference", path="/anthropic/v1/messages",
                     models="/v1/models", note="One key reaches Claude, GPT, Gemini, Kimi, GLM, "
                     "Qwen and more; non-Claude models are translated by OpenCode. Free models "
                     "only work inside OpenCode itself."),
    "opencode-go": dict(base="https://opencode.ai/inference/go", path="/anthropic/v1/messages",
                        models="/v1/models"),
    "openrouter": dict(base="https://openrouter.ai", path="/api/v1/messages",
                       models="/api/v1/models", note="Model ids are vendor/model."),
    "deepseek": dict(base="https://api.deepseek.com", path="/anthropic/v1/messages",
                     models="/models"),
    "fireworks-ai": dict(base="https://api.fireworks.ai/inference", path="/v1/messages",
                         models="/v1/models"),
    "moonshotai": dict(base="https://api.moonshot.ai", path="/anthropic/v1/messages",
                       models="/v1/models"),
    "zai": dict(base="https://api.z.ai/api", path="/anthropic/v1/messages",
                models="/paas/v4/models"),
    "zai-coding-plan": dict(base="https://api.z.ai/api", path="/anthropic/v1/messages",
                            models="/coding/paas/v4/models"),
    "minimax": dict(base="https://api.minimax.io", path="/anthropic/v1/messages",
                    models="/v1/models", note="Only MiniMax-M3 and MiniMax-M3.1-Flash answer "
                    "on the Anthropic route."),
    "minimax-coding-plan": dict(base="https://api.minimax.io", path="/anthropic/v1/messages",
                                models="/v1/models"),
    "minimax-cn": dict(base="https://api.minimax.cn", path="/anthropic/v1/messages",
                       models="/v1/models"),
    "minimax-cn-coding-plan": dict(base="https://api.minimax.cn", path="/anthropic/v1/messages",
                                   models="/v1/models"),
    "alibaba": dict(base="https://dashscope-intl.aliyuncs.com", path="/apps/anthropic/v1/messages",
                    models="/compatible-mode/v1/models", note="Each Model Studio plan has its "
                    "own host and key type; edit the base URL if your plan differs."),
    "alibaba-cn": dict(base="https://dashscope.aliyuncs.com", path="/apps/anthropic/v1/messages",
                       models="/compatible-mode/v1/models"),
    "alibaba-coding-plan": dict(base="https://coding-intl.dashscope.aliyuncs.com",
                                path="/apps/anthropic/v1/messages", models="/v1/models"),
    "alibaba-coding-plan-cn": dict(base="https://coding.dashscope.aliyuncs.com",
                                   path="/apps/anthropic/v1/messages", models="/v1/models"),
    "alibaba-token-plan": dict(base="https://token-plan.ap-southeast-1.maas.aliyuncs.com",
                               path="/apps/anthropic/v1/messages",
                               models="/compatible-mode/v1/models"),
    "alibaba-token-plan-cn": dict(base="https://token-plan.cn-beijing.maas.aliyuncs.com",
                                  path="/apps/anthropic/v1/messages",
                                  models="/compatible-mode/v1/models"),
    "ollama-cloud": dict(base="https://ollama.com", path="/v1/messages", models="/v1/models",
                         headers={"anthropic-version": "2023-06-01"}),
    "vercel": dict(base="https://ai-gateway.vercel.sh", path="/v1/messages", models="/v1/models"),
    "meta": dict(base="https://api.meta.ai", path="/v1/messages", models="/v1/models"),
    "lmstudio": dict(base="http://127.0.0.1:1234", path="/v1/messages", models="/v1/models",
                     auth="none", note="Needs LM Studio 0.4.1 or newer."),
    "github-copilot": dict(base="https://api.githubcopilot.com", path="/v1/messages",
                           models="/models", auth_class="oauth-device",
                           note="Claude models answer on /v1/messages. Sign-in uses GitHub's "
                           "device flow."),
    "azure": dict(base="https://{resource}.services.ai.azure.com", path="/anthropic/v1/messages",
                  models="/anthropic/v1/models", auth="x-api-key",
                  note="Claude deployments only; replace {resource} and use your deployment "
                  "name as the model."),
    "azure-cognitive-services": dict(base="https://{resource}.services.ai.azure.com",
                                     path="/anthropic/v1/messages",
                                     models="/anthropic/v1/models", auth="x-api-key",
                                     note="Claude deployments only; replace {resource}."),
}
# Catalog rows whose npm package is the Anthropic SDK and need no hand entry.
ANTHROPIC_SDK_AUTH = "x-api-key"

# Servers that run on this machine and are missing from models.dev.
LOCAL_EXTRA = {
    "ollama": dict(name="Ollama", base="http://127.0.0.1:11434", doc="https://docs.ollama.com/api/anthropic-compatibility",
                   note="Needs Ollama 0.14 or newer for /v1/messages.", aliases=["ollama-local"]),
    "llama.cpp": dict(name="llama.cpp (llama-server)", base="http://127.0.0.1:9931",
                      doc="https://github.com/ggml-org/llama.cpp/tree/master/tools/server",
                      note="llama-server listens on 9931 since October 2026 (8080 before). "
                      "Tools need --jinja, which is on by default.",
                      aliases=["llamacpp", "llama-server"], ports=[9931, 8080]),
    "vllm": dict(name="vLLM", base="http://127.0.0.1:8000", doc="https://docs.vllm.ai/en/latest/",
                 note="Tool calls need --enable-auto-tool-choice --tool-call-parser <name>."),
}

# OpenAI Chat Completions endpoints for providers whose models.dev package
# is not the generic openai-compatible one.
OPENAI_CHAT = {
    "openai": dict(base="https://api.openai.com", path="/v1/chat/completions",
                   quirks=dict(max_tokens_field="max_completion_tokens", reasoning="openai",
                               system_role="developer")),
    "groq": dict(base="https://api.groq.com/openai", path="/v1/chat/completions"),
    "mistral": dict(base="https://api.mistral.ai", path="/v1/chat/completions",
                    quirks=dict(tool_ids="short")),
    "cerebras": dict(base="https://api.cerebras.ai", path="/v1/chat/completions",
                     headers={"x-cerebras-3rd-party-integration": "batbox"}),
    "togetherai": dict(base="https://api.together.xyz", path="/v1/chat/completions"),
    "xai": dict(base="https://api.x.ai", path="/v1/chat/completions",
                quirks=dict(reasoning="reasoning_content")),
    "deepinfra": dict(base="https://api.deepinfra.com/v1/openai", path="/chat/completions",
                      models="/models"),
    "google": dict(base="https://generativelanguage.googleapis.com/v1beta/openai",
                   path="/chat/completions", models="/models",
                   quirks=dict(reasoning="openai"),
                   note="Uses Gemini's OpenAI-compatible layer."),
}
QUIRK_OVERRIDES = {
    "snowflake-cortex": dict(max_tokens_field="max_completion_tokens"),
    "moonshotai-cn": dict(reasoning="reasoning_content"),
}

CLOUD = {
    "amazon-bedrock": dict(kind="bedrock", base="https://bedrock-runtime.us-east-1.amazonaws.com",
                           path="/model/{model}/invoke-with-response-stream", auth_class="aws",
                           note="Signs requests with your AWS credentials (environment, "
                           "AWS_PROFILE or SSO) or AWS_BEARER_TOKEN_BEDROCK. Edit the region "
                           "in the URL."),
    "google-vertex": dict(kind="vertex", base="https://us-east5-aiplatform.googleapis.com",
                          path="/v1/projects/{project}/locations/us-east5/publishers/anthropic/models/{model}:streamRawPredict",
                          auth_class="gcp", note="Claude models through Vertex AI with "
                          "Application Default Credentials (gcloud auth application-default "
                          "login). Set GOOGLE_VERTEX_PROJECT."),
    "google-vertex-anthropic": dict(kind="vertex", base="https://us-east5-aiplatform.googleapis.com",
                                    path="/v1/projects/{project}/locations/us-east5/publishers/anthropic/models/{model}:streamRawPredict",
                                    auth_class="gcp", note="Same as google-vertex, Claude only."),
}

# Hand annotations from the provider research: does the provider expose an
# Anthropic Messages endpoint? Everything unlisted is "unverified".
ANTHROPIC_NATIVE = {
    "yes": ["alibaba", "alibaba-cn", "alibaba-coding-plan", "alibaba-coding-plan-cn",
            "alibaba-token-plan", "alibaba-token-plan-cn", "amazon-bedrock", "anthropic", "azure",
            "azure-cognitive-services", "deepseek", "fireworks-ai", "freemodel", "github-copilot",
            "google-vertex", "google-vertex-anthropic", "lmstudio", "meta", "minimax",
            "minimax-cn", "minimax-cn-coding-plan", "minimax-coding-plan", "moonshotai",
            "ollama-cloud", "opencode", "opencode-go", "openrouter", "subconscious",
            "thinkingmachines", "vercel", "zai", "zai-coding-plan", "ollama", "llama.cpp",
            "vllm"],
    "likely": ["agentrouter", "aixy", "cloudflare-ai-gateway", "commandcode", "ofox", "vivgrid",
               "zenmux"],
    "no": ["cerebras", "gitlab", "google", "groq", "mistral", "openai"],
}

PRIORITY = ["opencode", "anthropic", "openrouter", "openai", "github-copilot", "google", "groq",
            "deepseek", "mistral", "xai", "togetherai", "fireworks-ai", "cerebras", "moonshotai",
            "zai", "minimax", "alibaba", "amazon-bedrock", "google-vertex",
            "ollama", "llama.cpp", "lmstudio", "vllm"]
ALIASES = {
    "opencode": ["zen", "opencode-zen"], "togetherai": ["together"],
    "fireworks-ai": ["fireworks"], "alibaba": ["dashscope", "qwen"],
    "amazon-bedrock": ["bedrock"], "google-vertex": ["vertex"],
    "github-copilot": ["copilot"], "moonshotai": ["moonshot", "kimi"],
    "zai": ["z.ai", "glm"], "google": ["gemini"], "xai": ["grok"],
}
DROP_TYPES = {"image", "embedding", "video", "speech", "transcription", "reranking",
              "realtime", "decision"}
KEY_HINTS = ("KEY", "TOKEN", "PAT", "SECRET")
# OpenCode's free models answer only inside OpenCode; other clients get 403.
HARNESS_ONLY = {"opencode", "opencode-go"}


def load(source):
    if source.startswith("https://"):
        request = urllib.request.Request(source, headers={"User-Agent": "batbox-catalog"})
        with urllib.request.urlopen(request, timeout=60) as response:
            return json.loads(response.read().decode("utf-8"))
    return json.loads(Path(source).read_text(encoding="utf-8"))


def native_flag(provider_id):
    for flag, ids in ANTHROPIC_NATIVE.items():
        if provider_id in ids:
            return flag
    return "unverified"


def is_local(url):
    return url.startswith("http://127.0.0.1") or url.startswith("http://localhost")


def split_api(api):
    api = api.rstrip("/")
    if api.endswith("/v1"):
        return api[:-3], "/v1/chat/completions", "/v1/models"
    return api, "/chat/completions", "/models"


def model_row(model):
    flags = ""
    if model.get("tool_call"):
        flags += "t"
    if model.get("reasoning"):
        flags += "r"
    inputs = (model.get("modalities") or {}).get("input") or []
    for name, letter in (("image", "i"), ("pdf", "p"), ("audio", "a"), ("video", "v")):
        if name in inputs:
            flags += letter
    limit = model.get("limit") or {}
    name = model.get("name") or model["id"]
    return [model["id"], "" if name == model["id"] else name,
            int(limit.get("context") or 0), int(limit.get("output") or 0), flags]


def pick_models(provider_id, models):
    rows = []
    for model in models.values():
        if model.get("status") == "deprecated" or model.get("type") in DROP_TYPES:
            continue
        if provider_id in HARNESS_ONLY and (model["id"].endswith("-free") or
                                            model["id"] == "big-pickle"):
            continue
        outputs = (model.get("modalities") or {}).get("output") or ["text"]
        if "text" not in outputs:
            continue
        rows.append(model)
    if len(rows) > MODEL_LIMIT:
        tools = [m for m in rows if m.get("tool_call")]
        if tools:
            rows = tools
    rows.sort(key=lambda m: m["id"])
    rows.sort(key=lambda m: m.get("last_updated") or m.get("release_date") or "", reverse=True)
    rows = rows[:MODEL_LIMIT]
    return [model_row(m) for m in rows]


def reasoning_quirk(models):
    fields = set()
    for model in models.values():
        interleaved = model.get("interleaved")
        if isinstance(interleaved, dict) and interleaved.get("field"):
            fields.add(interleaved["field"])
    if "reasoning_details" in fields:
        return "openrouter"
    if "reasoning_content" in fields:
        return "reasoning_content"
    return "none"


def base_quirks(models):
    return {"max_tokens_field": "max_tokens", "reasoning": reasoning_quirk(models),
            "tool_ids": "any", "usage_in_stream": True, "system_role": "system"}


def key_env(env):
    return [name for name in env if any(hint in name for hint in KEY_HINTS)
            and name != "GOOGLE_APPLICATION_CREDENTIALS"]


def entry(provider_id, provider):
    npm = provider.get("npm", "")
    models = provider.get("models") or {}
    env = list(provider.get("env") or [])
    row = {"id": provider_id, "name": provider.get("name") or provider_id,
           "anthropic_native": native_flag(provider_id), "doc": provider.get("doc", ""),
           "key_env": key_env(env), "auth": "key", "auth_header": "authorization"}
    config_env = [name for name in env if name not in row["key_env"]]
    if config_env:
        row["config_env"] = config_env
    api = (provider.get("api") or "").rstrip("/")
    if provider_id in ANTHROPIC:
        spec = ANTHROPIC[provider_id]
        row.update(kind="anthropic", base_url=spec["base"], messages_path=spec["path"],
                   models_path=spec.get("models", "/v1/models"),
                   auth_header=spec.get("auth", "authorization"))
        if spec.get("auth") == "none":
            row["auth"] = "none"
        if spec.get("auth_class"):
            row["auth"] = spec["auth_class"]
        if spec.get("headers"):
            row["headers"] = spec["headers"]
        if spec.get("note"):
            row["note"] = spec["note"]
    elif npm == "@ai-sdk/anthropic" and api:
        base = api[:-3] if api.endswith("/v1") else api
        row.update(kind="anthropic", base_url=base, messages_path="/v1/messages",
                   models_path="/v1/models", auth_header=ANTHROPIC_SDK_AUTH)
    elif provider_id in CLOUD:
        spec = CLOUD[provider_id]
        row.update(kind=spec["kind"], base_url=spec["base"], messages_path=spec["path"],
                   models_path="", auth=spec["auth_class"], auth_header="none",
                   note=spec["note"])
    elif provider_id in OPENAI_CHAT or (npm == "@ai-sdk/openai-compatible" and api):
        spec = OPENAI_CHAT.get(provider_id)
        if spec:
            base, path = spec["base"], spec["path"]
            models_path = spec.get("models", "/v1/models")
        else:
            base, path, models_path = split_api(api)
        quirks = base_quirks(models)
        if spec and spec.get("quirks"):
            quirks.update(spec["quirks"])
        quirks.update(QUIRK_OVERRIDES.get(provider_id, {}))
        row.update(kind="openai-chat", base_url=base, messages_path=path,
                   models_path=models_path, quirks=quirks)
        if spec and spec.get("headers"):
            row["headers"] = spec["headers"]
        if spec and spec.get("note"):
            row["note"] = spec["note"]
        if is_local(base):
            row["auth"] = "none"
            row["auth_header"] = "none"
    else:
        reason = ("Speaks the OpenAI Responses API; Batbox translates Chat Completions only."
                  if npm == "@ai-sdk/openai" else
                  "Uses its own protocol (" + (npm or "unknown") + "); not supported yet.")
        if provider_id == "gitlab":
            reason = "GitLab Duo uses its own agent protocol; not supported."
        row.update(kind="unsupported", base_url=api, messages_path="", models_path="",
                   reason=reason)
    if "${" in row.get("base_url", ""):
        row["base_url_needs"] = sorted({part.split("}")[0] for part in row["base_url"].split("${")[1:]})
    if row["key_env"] == [] and row["auth"] == "key" and row["kind"] != "unsupported":
        row["auth"] = "none" if is_local(row.get("base_url", "")) else "key"
    if provider_id in ALIASES:
        row["aliases"] = ALIASES[provider_id]
    row["models"] = pick_models(provider_id, models)
    return row


def local_entry(provider_id, spec):
    row = {"id": provider_id, "name": spec["name"], "kind": "anthropic",
           "anthropic_native": native_flag(provider_id), "doc": spec["doc"], "key_env": [], "auth": "none", "auth_header": "none", "base_url": spec["base"],
           "messages_path": "/v1/messages", "models_path": "/v1/models", "note": spec["note"],
           "local": True, "models": []}
    if spec.get("aliases"):
        row["aliases"] = spec["aliases"]
    if spec.get("ports"):
        row["ports"] = spec["ports"]
    return row


def build(catalog):
    rows = {pid: entry(pid, provider) for pid, provider in catalog.items()}
    for pid, spec in LOCAL_EXTRA.items():
        rows.setdefault(pid, local_entry(pid, spec))
    for pid, row in rows.items():
        if is_local(row.get("base_url", "")):
            row["local"] = True
    order = {pid: index for index, pid in enumerate(PRIORITY)}
    ordered = sorted(rows.values(), key=lambda r: (order.get(r["id"], len(order)),
                                                    r["name"].lower(), r["id"]))
    return {"source": "models.dev api.json",
            "defaults": {"context": DEFAULT_CONTEXT, "output": DEFAULT_OUTPUT},
            "providers": ordered}


def emit(document, source_label):
    text = json.dumps(document, separators=(",", ":"), ensure_ascii=False, sort_keys=False)
    data = text.encode("utf-8")
    chunks, start = [], 0
    while start < len(data):
        end = min(start + CHUNK, len(data))
        while end < len(data) and (data[end] & 0xC0) == 0x80:
            end -= 1
        chunks.append(data[start:end].decode("utf-8"))
        start = end
    for chunk in chunks:
        if ")BBX\"" in chunk:
            raise SystemExit("catalog text contains the raw-string delimiter")
    lines = ["// Generated by gadgets/providers_catalog.py from " + source_label + ". Do not edit.",
             "// Regenerate: python3 gadgets/providers_catalog.py <api.json>",
             "#include \"providers/Catalog.hpp\"", "",
             "namespace batbox::providers {", "namespace {",
             "constexpr const char* kChunks[]={"]
    for chunk in chunks:
        lines.append("R\"BBX(" + chunk + ")BBX\",")
    lines += ["};", "}", "",
              "std::size_t catalogChunkCount(){return sizeof(kChunks)/sizeof(kChunks[0]);}",
              "const char* catalogChunk(std::size_t index){return kChunks[index];}",
              "std::size_t catalogBytes(){return " + str(len(data)) + ";}",
              "}", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("source", help="path or https URL of a models.dev api.json")
    parser.add_argument("--output", default=str(Path(__file__).resolve().parent.parent /
                                                "src/providers/Catalog.generated.cpp"))
    parser.add_argument("--stdout", action="store_true", help="write the C++ to stdout")
    arguments = parser.parse_args()
    document = build(load(arguments.source))
    label = arguments.source if arguments.source.startswith("https://") else "models.dev api.json"
    output = emit(document, label)
    if arguments.stdout:
        sys.stdout.write(output)
        return 0
    Path(arguments.output).write_text(output, encoding="utf-8", newline="\n")
    count = len(document["providers"])
    models = sum(len(row["models"]) for row in document["providers"])
    print(f"{arguments.output}: {count} providers, {models} models, "
          f"{len(output.encode('utf-8'))} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())

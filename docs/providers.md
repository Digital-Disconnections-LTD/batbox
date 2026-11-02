# Providers 🦇

Batbox ships with a catalog of 238 model providers: the 235 listed by
[models.dev](https://models.dev) plus Ollama, llama.cpp and vLLM, which run on
your own machine. 223 of them work today. Pick one, give it a key (or let Batbox
find the key in your environment), choose a model, and you are flying.

Servers that speak Anthropic Messages, such as Anthropic, OpenCode Zen,
OpenRouter, DeepSeek, Ollama, llama.cpp, LM Studio and vLLM, are presets: Batbox
talks to them directly. Servers that only speak OpenAI Chat Completions, such as
OpenAI, Groq, Mistral and most gateways, go through Batbox's translator. Amazon
Bedrock and Google Vertex use their own signed requests.

## Pick a provider with /providers

Batbox opens `/providers` by itself the first time you start it with nothing
configured. Afterwards, type `/providers` (or `/connect`) whenever you want to
add another server.

1. **Choose a server.** The list has five groups:
   - *Found on this machine*: model servers that answered on their usual ports
     (Ollama 11434, LM Studio 1234, llama-server 9931 and 8080, vLLM 8000), with
     the models they report.
   - *Configured*: endpoints you already saved. Pick one to change its key, URL
     or model.
   - *Cloud*, *Local servers* and *Unsupported yet*: the catalog. Unsupported
     entries say why when you select them.

   Type to search by name or id (`zen`, `router`, `llama`). Backspace edits the
   search, Ctrl+U clears it, ↑ and ↓ move, Enter picks, Esc closes.
2. **Give it a key.** If one of the provider's key variables is set, for example
   `OPENROUTER_API_KEY`, Batbox offers **Use $OPENROUTER_API_KEY** and saves only
   the variable name. Otherwise a hidden field asks for the key, which is saved in
   `native-client.json` with permissions only you can read. Type `$NAME` in that
   field to use a different variable. Local servers skip this step; add a key
   later in `/config` if yours has one.
3. **Check the base URL.** It comes from the catalog. Edit it for a self-hosted,
   regional or non-default port, then press Enter.
4. **Test the connection.** Batbox lists the server's models with a plain GET. It
   never sends a chat request here, so the test costs nothing. If it fails you get
   a readable reason (wrong key, nothing listening, no model list) and can retry,
   edit the URL, change the key, or carry on with the catalog's models.
5. **Pick a model.** The list merges what the server reported with what the
   catalog knows: context and output limits, and whether the model does tools and
   reasoning. Type to search, or choose **Enter a model ID…** for anything else.

Batbox saves the endpoint, switches to it, and tells you:

```text
Added openrouter · qwen/qwen3-coder · /model to switch
```

The endpoint keeps the server's other models, so `/model` switches between them.
`/config` edits every field, including the limits and capability switches.

Endpoint ids follow the catalog id (`openrouter`, `llama.cpp`). Adding the same
provider again creates `openrouter-2`, so a second account or a second machine
never overwrites the first.

## Set one up from a script

`batbox providers add` writes the same endpoint without the terminal interface:

```sh
# A local server: no key, the first model it reports.
batbox providers add ollama

# A key from your environment (only the variable name is saved).
batbox providers add openrouter --key-env OPENROUTER_API_KEY --model qwen/qwen3-coder

# A key from stdin, so it stays out of shell history.
printf '%s\n' "$MY_KEY" | batbox providers add anthropic --key - --model claude-sonnet-5-5

# Your own host or port.
batbox providers add llama.cpp --base-url http://gpu-box.lan:8080
```

| Option | Meaning |
| --- | --- |
| `--key-env VAR` | Read the key from `VAR` at request time. Defaults to the catalog's variable when it is set. |
| `--key -` | Read the key from the first line of stdin and store it privately. |
| `--base-url URL` | Replace the catalog URL. Required when the catalog URL has a placeholder. |
| `--model ID` | The model to select. Defaults to the first model the server lists, then the catalog's first. |
| `--id NAME` | Save under this endpoint id instead of the catalog id. |
| `--no-test` | Skip the model-list request. |

`batbox providers` lists local servers, your endpoints and the whole catalog.
`batbox providers show zen` prints one provider's URL, paths, key variable and
models. `batbox doctor` includes local servers and configured endpoints.

## Quick notes

| Provider | Notes |
| --- | --- |
| **OpenCode Zen** (`opencode`) | One key from [opencode.ai/auth](https://opencode.ai/auth) reaches Claude, GPT, Gemini, Kimi, GLM, Qwen and more over `https://opencode.ai/inference/anthropic/v1/messages`. OpenCode translates non-Claude models. Its free models only answer inside OpenCode, so the catalog leaves them out. Key: `OPENCODE_API_KEY`. |
| **Anthropic** (`anthropic`) | `x-api-key` with `ANTHROPIC_API_KEY`. Thinking, effort and prompt caching start on for models that support them. |
| **OpenRouter** (`openrouter`) | Uses OpenRouter's Messages route at `/api/v1/messages`. Model ids look like `vendor/model`. Key: `OPENROUTER_API_KEY`. |
| **OpenAI** (`openai`) | Chat Completions through the translator, with `max_completion_tokens` and the `developer` role. Key: `OPENAI_API_KEY`. |
| **Ollama** (`ollama`) | Needs Ollama 0.14 or newer for `/v1/messages`. No key locally. `ollama pull qwen3-coder` first; the picker shows every pulled model. Ollama Cloud is its own entry (`ollama-cloud`). |
| **llama.cpp** (`llama.cpp`) | Start `llama-server -m model.gguf` (tools need `--jinja`, which is on by default). Since October 2026 it listens on 9931; older builds use 8080, and the picker checks both. For a server started with `--api-key`, add the key with `batbox providers add llama.cpp --key -` or in `/config`. |
| **LM Studio** (`lmstudio`) | Turn on the local server in LM Studio 0.4.1 or newer. Port 1234, no key unless you enabled authentication. |
| **vLLM** (`vllm`) | `vllm serve <model> --enable-auto-tool-choice --tool-call-parser <parser>` so tool calls work. Port 8000. If you start vLLM with `--api-key`, add the same key with `batbox providers add vllm --key -` or in `/config`. |
| **DeepSeek** (`deepseek`) | Messages at `https://api.deepseek.com/anthropic`. Key: `DEEPSEEK_API_KEY`. |
| **Groq**, **Mistral**, **Cerebras**, **Together AI**, **xAI** | Chat Completions through the translator. Each reads its usual `*_API_KEY` variable. |
| **Moonshot / Kimi**, **Z.AI / GLM**, **MiniMax**, **Alibaba / Qwen** | Each has an Anthropic route, used directly. Alibaba plans have different hosts and keys; edit the base URL to match yours. MiniMax serves only M3 and M3.1-Flash on that route. |
| **Amazon Bedrock**, **Google Vertex** | Signed with your AWS or Google credentials instead of a key. Edit the region in the base URL. |
| **GitHub Copilot** | Listed as coming soon: it needs GitHub's device sign-in. |

## How Batbox talks to a server

The `provider` field of each endpoint names the wire protocol: `anthropic` (the
default) sends Anthropic Messages to `base_url` plus `messages_path`;
`openai-chat` sends Chat Completions to `base_url` plus `messages_path` and
translates the stream back; `bedrock` and `vertex` sign each request. `preset`
records the catalog id, and `quirks` holds the per-server Chat Completions
details the catalog chose.

## Where the catalog comes from

`gadgets/providers_catalog.py` reads a models.dev `api.json` and writes
`src/providers/Catalog.generated.cpp`. It keeps, per provider, the wire kind,
base URL, paths, key variables, documentation link, default quirks and up to 40
current, tool-capable models with their limits. To refresh it:

```sh
python3 gadgets/providers_catalog.py https://models.dev/api.json
```

The catalog is compiled in. Batbox reads models.dev only when you run that
script, never at startup.

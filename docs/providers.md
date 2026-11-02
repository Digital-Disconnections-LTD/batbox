# Providers

## How Batbox talks to a server

Every endpoint and profile has a `provider` kind. The kind picks the wire
format used for that endpoint. Everything above the wire (transcripts, tools,
compaction, resume, `--print` output) is the same whichever kind an endpoint
uses, because each kind turns its server's replies into Anthropic Messages
events before the rest of Batbox sees them.

| `provider` | Wire format | Default `messages_path` |
|---|---|---|
| `anthropic` (default) | Anthropic Messages with SSE streaming | `/v1/messages` |
| `openai-chat` | OpenAI Chat Completions with SSE streaming, translated | `/v1/chat/completions` |
| `bedrock` | AWS Bedrock `InvokeModelWithResponseStream` | set by the Bedrock adapter |
| `vertex` | Google Vertex `rawPredict` / `streamRawPredict` | set by the Vertex adapter |

An endpoint without `provider` is `anthropic`, so existing configurations keep
working unchanged. Any other value is a configuration error that lists the
allowed kinds. A build that doesn't include the Bedrock or Vertex adapter
accepts those kinds in configuration and fails the request with "Provider
bedrock is not available in this build of Batbox".

Use `anthropic` for any server with a working `/v1/messages` route, including
recent vLLM, llama.cpp `llama-server`, Ollama and LM Studio. Use `openai-chat`
for servers that only offer `/v1/chat/completions`. One server can back two
endpoints, one of each kind, with the same model.

### Endpoint example

```json
{
  "profile": "local-chat::your-model",
  "endpoints": {
    "local-chat": {
      "label": "llama-server",
      "provider": "openai-chat",
      "preset": "llama.cpp",
      "base_url": "http://127.0.0.1:9931",
      "auth_header": "none",
      "context_window": 32768,
      "max_output_tokens": 4096,
      "models": ["your-model"],
      "quirks": { "reasoning": "reasoning_content" }
    }
  }
}
```

The same keys work in a `profiles` entry, which also takes `model` and
`api_key_helper`.

### Fields

| Key | Meaning |
|---|---|
| `provider` | Wire kind from the table above. Default `anthropic`. |
| `preset` | Catalog id the endpoint came from, such as `vllm`, `ollama`, `openrouter` or `llama.cpp`. Shown in the UI; it doesn't change requests. |
| `base_url` | Server root, `http` or `https`. Credentials don't belong in the URL. |
| `messages_path` | Request path. For `openai-chat` this is the chat completions path. When `base_url` already ends in `/v1`, a leading `/v1` is dropped, so `/v1/chat/completions` becomes `/chat/completions`. |
| `models_path` | Model list path. Without it Batbox uses `/v1/models`, or `/models` when `base_url` ends in `/v1`. The reply must be `{"data":[{"id":...}]}`. |
| `auth_header` | `authorization` sends `Authorization: Bearer <key>`. `x-api-key` sends the key in `x-api-key` (and adds `anthropic-version: 2023-06-01` for `anthropic` endpoints). `none` sends no credential. |
| `auth_token`, `api_key_env` | The key itself, or the name of an environment variable that holds it. `auth_token` wins when both are set. |
| `headers` | Extra request headers, single-line text values. |
| `models` | Model ids offered by this endpoint. Each becomes a profile named `<endpoint>::<model>`. |
| `context_window`, `max_output_tokens` | Required token limits. Output must not exceed context. |
| `thinking`, `prompt_caching`, `adaptive_thinking`, `structured_outputs`, `effort`, `send_disabled_thinking` | Capability flags. They control which request fields Batbox builds. On `openai-chat`, thinking only reaches the server through the `reasoning` quirk below. |
| `quirks` | `openai-chat` only. See the next section. |

### Quirks for `openai-chat`

Servers that speak Chat Completions disagree on a few details. `quirks` holds
the per-server choices. Absent keys use the default. An unknown key or value is
a configuration error.

| Key | Values | Default | Effect |
|---|---|---|---|
| `max_tokens_field` | `max_tokens`, `max_completion_tokens` | `max_tokens` | Name of the output limit field. OpenAI and Azure want `max_completion_tokens`; most other servers want `max_tokens`. |
| `reasoning` | `none`, `openai`, `reasoning_content`, `openrouter` | `none` | How a thinking request is sent and how earlier thinking is replayed. See below. |
| `tool_ids` | `any`, `short` | `any` | `any` sends Batbox's tool ids, replacing any id longer than 40 characters. `short` sends exactly nine letters and digits, which Mistral-family servers require. |
| `usage_in_stream` | `true`, `false` | `true` | Sends `stream_options: {"include_usage": true}`. Turn it off for a server that rejects the field. Batbox then estimates usage at four bytes per token. |
| `system_role` | `system`, `developer` | `system` | Role of the message that carries the system prompt. OpenAI reasoning models use `developer`. |

`reasoning` values:

- `none`: the thinking request is dropped and earlier thinking isn't sent back.
- `openai`: sends `reasoning_effort`. An explicit effort level is used as is
  (`max` becomes `high`). Otherwise the thinking budget picks it: 10000 tokens
  or more is `high`, 5000 or more is `medium`, anything smaller is `low`.
- `reasoning_content`: sends `chat_template_kwargs: {"enable_thinking": true}`
  (or `false` when thinking is turned off), which vLLM, SGLang and llama.cpp
  pass to templates such as Qwen3. Earlier thinking goes back as
  `reasoning_content` on assistant messages.
- `openrouter`: sends `reasoning: {"max_tokens": N}` or
  `reasoning: {"effort": ...}`, and replays earlier thinking as `reasoning`.

Whatever the setting, `reasoning_content` or `reasoning` text in a reply is
shown as a thinking block.

### What the `openai-chat` translation does

Requests:

- System text blocks are joined with blank lines into one `system` (or
  `developer`) message.
- User text and images become message content. Base64 images are sent as data
  URLs. PDF documents are replaced by a short note, because Chat Completions
  servers generally can't read them.
- Each `tool_result` becomes a `tool` message right after the assistant
  message that made the call. Errors are prefixed with `Error: `. Images
  returned by a tool are moved into a user message after the tool messages.
- Assistant `tool_use` blocks become `tool_calls` with JSON string arguments.
- Tools become `function` tools with `parameters` set to the input schema.
  Server-side tools and `cache_control` are dropped.
- `tool_choice` maps `auto`, `any`, `none` and a named tool to `auto`,
  `required`, `none` and a named function. `disable_parallel_tool_use` becomes
  `parallel_tool_calls: false`.
- `stop_sequences` becomes `stop`. `temperature` and `top_p` are copied.
  `top_k` is dropped. A structured output format becomes `response_format`
  with a JSON schema.

Replies:

- Text, reasoning and tool call deltas stream as Anthropic content blocks.
  Tool arguments stream as they arrive.
- `finish_reason` maps `stop` to `end_turn`, `length` to `max_tokens`,
  `tool_calls` to `tool_use` and `content_filter` to `refusal`. A reply with
  tool calls ends in `tool_use` unless it ran out of output tokens.
- Usage comes from the final usage chunk. Cached prompt tokens are reported as
  cache reads and subtracted from input tokens.
- The reply's model is the configured model id, so `--resume` finds the right
  profile again.
- Server tool call ids are kept when they are 8 to 40 characters of letters,
  digits, `_` and `-`. Short, empty or repeated ids are replaced with fresh
  `toolu_` ids.
- A broken stream (unparsable chunk, end before a finish reason, a tool call
  with no name) makes Batbox repeat the turn without streaming, unless
  `BATBOX_DISABLE_NONSTREAMING_FALLBACK` is set.

Errors keep their HTTP status and are reported as Anthropic error types:
401 and 403 as `authentication_error`, 429 as `rate_limit_error`, 503 as
`overloaded_error`, other 5xx as `api_error` and other 4xx as
`invalid_request_error`. When vLLM or OpenAI reports that input plus
`max_tokens` exceeds the context length, Batbox lowers `max_tokens` and tries
again. A prompt that alone exceeds the context (including llama.cpp's
`exceed_context_size_error`) is reported as "prompt is too long".

### Retries

A 401 or 403 is never retried. The error names the server and the setting to
check, for example `401 from http://127.0.0.1:9931: check auth_token`, followed
by the server's own message.

Endpoints whose `provider` isn't `anthropic`, and any endpoint on `localhost`,
`127.x.x.x`, `::1` or `0.0.0.0`, retry at most twice and never wait more than
2 seconds between attempts, including when the server sends `retry-after`. A
local server that is down or misconfigured fails within a few seconds.
`BATBOX_MAX_RETRIES` overrides the count. Other `anthropic` endpoints keep the
standard ladder of 10 retries with backoff up to 32 seconds.

### Not carried over to `openai-chat`

- Prompt caching breakpoints. Servers with automatic prefix caching still
  cache.
- Thinking signatures and redacted thinking.
- PDF documents and server-side tools.
- Parallel tool calls on servers that interleave the arguments of several
  calls in one stream. Batbox detects this and repeats the turn without
  streaming.

## Amazon Bedrock

Batbox calls Anthropic models on Bedrock through `InvokeModelWithResponseStream`
(and `InvokeModel` when a request is not streamed). The request body is the
Messages API body without `model`, with `anthropic_version: "bedrock-2023-05-31"`.
Bedrock's binary event stream is decoded back into Messages API stream events, so
Flights, gadgets and resume work as they do with any other endpoint.

### Endpoint record

```json
{
  "profile": "bedrock::us.anthropic.claude-opus-5-5-v1:0",
  "endpoints": {
    "bedrock": {
      "label": "Amazon Bedrock",
      "provider": "bedrock",
      "region": "us-west-2",
      "models": ["us.anthropic.claude-opus-5-5-v1:0"],
      "context_window": 1000000,
      "max_output_tokens": 128000
    }
  }
}
```

- `region` picks `https://bedrock-runtime.{region}.amazonaws.com`. Without it,
  Batbox uses `AWS_REGION`, then `AWS_DEFAULT_REGION`, then the profile's
  `region` in `~/.aws/config`, then `us-east-1`.
- `base_url` is optional. Set it to reach Bedrock Runtime through a VPC interface
  endpoint, for example
  `https://vpce-0123-abcd.bedrock-runtime.us-west-2.vpce.amazonaws.com`.
  `AWS_ENDPOINT_URL_BEDROCK_RUNTIME` and `AWS_ENDPOINT_URL` work too.
- Model ids are Bedrock ids: an inference profile such as
  `us.anthropic.claude-opus-5-5-v1:0` or `global.anthropic.claude-sonnet-5-5-v1:0`,
  a base model id, or an ARN. Newer Claude models are only offered through
  inference profiles.
- **Discover models** lists the region's Anthropic inference profiles and the
  Anthropic base models that support on-demand use. It calls
  `https://bedrock.{region}.amazonaws.com` (`AWS_ENDPOINT_URL_BEDROCK` overrides it).

### Credentials

Batbox looks for credentials in this order and uses the first it finds:

1. The endpoint's API key (`auth_token`, `api_key_env` or `api_key_helper`), sent
   as a Bedrock API key.
2. `AWS_BEARER_TOKEN_BEDROCK`, a Bedrock API key. Requests carry it as a bearer
   token and are not signed.
3. `AWS_ACCESS_KEY_ID` and `AWS_SECRET_ACCESS_KEY`, with `AWS_SESSION_TOKEN` for
   temporary credentials. Requests are signed with Signature Version 4.
4. The profile named by `AWS_PROFILE` (or `default`) in `~/.aws/credentials` and
   `~/.aws/config` (`AWS_SHARED_CREDENTIALS_FILE` and `AWS_CONFIG_FILE` move them):
   - `role_arn` with `source_profile` (or `credential_source = Environment`):
     Batbox calls STS `AssumeRole` with the source credentials.
     `role_session_name`, `duration_seconds` and `external_id` are honoured.
     `AWS_ENDPOINT_URL_STS` overrides the STS endpoint.
   - `aws_access_key_id` and `aws_secret_access_key`, with optional
     `aws_session_token`.
   - `credential_process`: the command's JSON output (`Version: 1`) is reused
     until a few minutes before its `Expiration`.

Resolved credentials stay in memory until five minutes before they expire. A
response saying the token expired clears them, and the request is sent once more.

IAM Identity Center (SSO) profiles go through `credential_process`. AWS CLI v2
prints SSO credentials in that format:

```ini
[profile bedrock]
region = us-west-2
credential_process = aws configure export-credentials --profile my-sso-profile --format process
```

Run `aws sso login --profile my-sso-profile` when the SSO session lapses.
Batbox does not read the EC2 instance metadata service, ECS container
credentials, or web identity tokens; on those hosts, use `credential_process`
with the AWS CLI command above.

### IAM

The calling identity needs:

- `bedrock:InvokeModelWithResponseStream` and `bedrock:InvokeModel` on the model
  or inference profile. For a cross-region inference profile, grant them on the
  profile ARN and on the underlying foundation models in each destination region.
- `bedrock:ListFoundationModels` and `bedrock:ListInferenceProfiles` for
  **Discover models**.
- `sts:AssumeRole` on the role, when a profile uses `role_arn`.

Anthropic models can require a one-time use case submission in the Bedrock
console before the first call. Until then, requests fail with
`AccessDeniedException`.

### Errors

Bedrock errors appear as Messages API errors, so retry and messages behave as
they do elsewhere. `ThrottlingException` and `ServiceQuotaExceededException` are
rate limit errors (429). `ModelNotReadyException` and
`ServiceUnavailableException` are overloaded errors and are retried.
`ValidationException` is an invalid request (400). `AccessDeniedException` is a
permission error (403). Expired or invalid credentials are authentication errors (401).

## Google Vertex AI

Batbox calls Claude on Vertex AI with `streamRawPredict` (and `rawPredict` for
requests that are not streamed). The body is the Messages API body without
`model`, with `anthropic_version: "vertex-2023-10-16"`. The response stream is
ordinary server-sent events.

### Endpoint record

```json
{
  "profile": "vertex::claude-opus-5-5",
  "endpoints": {
    "vertex": {
      "label": "Vertex AI",
      "provider": "vertex",
      "project": "my-gcp-project",
      "region": "global",
      "models": ["claude-opus-5-5", "claude-sonnet-5-5", "claude-haiku-5-5"],
      "context_window": 1000000,
      "max_output_tokens": 128000
    }
  }
}
```

- `project` is required. Without it, Batbox uses `GOOGLE_VERTEX_PROJECT`,
  `GOOGLE_CLOUD_PROJECT`, `GCP_PROJECT`, `GCLOUD_PROJECT`, then the `project_id`
  in the credentials file.
- `region` is a Vertex location such as `us-east5` or `europe-west1`, or `global`.
  `global` uses `https://aiplatform.googleapis.com`; any other region uses
  `https://{region}-aiplatform.googleapis.com`. Without it, Batbox uses
  `GOOGLE_VERTEX_LOCATION`, `GOOGLE_CLOUD_LOCATION`, `VERTEX_LOCATION`, then `global`.
- `base_url` is optional and replaces the host, for example a Private Service
  Connect endpoint.
- Vertex has no inexpensive way to list Anthropic models, so **Discover models**
  returns the endpoint's `models`, or Batbox's built-in list of Claude model ids
  when `models` is empty. Model ids are the Vertex names, such as
  `claude-sonnet-5-5`; dated names such as `claude-3-5-sonnet-v2@20241022` also work.

### Credentials

Batbox mints an OAuth access token for the `cloud-platform` scope, in this order:

1. The endpoint's API key (`auth_token`, `api_key_env` or `api_key_helper`), used
   as a ready access token. `"api_key_helper": "gcloud auth print-access-token"`
   is one way to supply it.
2. `GOOGLE_APPLICATION_CREDENTIALS`, pointing at a service-account key file
   (Batbox signs a JWT and exchanges it at the file's `token_uri`) or at an
   `authorized_user` file.
3. The file `gcloud auth application-default login` writes
   (`~/.config/gcloud/application_default_credentials.json`, or under
   `CLOUDSDK_CONFIG`).
4. `gcloud auth print-access-token`.
5. The metadata server on Compute Engine, GKE and Cloud Run
   (`GCE_METADATA_HOST` overrides its address).

Tokens are reused until a minute before they expire. A token from `gcloud` is
reused for fifteen minutes. A 401 response discards the token, and the request
is sent once more.

### IAM

- Enable the Vertex AI API in the project and enable each Claude model you use
  from its Model Garden page.
- Give the calling identity the Vertex AI User role (`roles/aiplatform.user`),
  or another role with `aiplatform.endpoints.predict`.
- Quotas are per model and region. Requests over quota return
  `RESOURCE_EXHAUSTED`, which Batbox reports as a rate limit error (429).

### Errors

Google RPC errors are mapped by status: `RESOURCE_EXHAUSTED` is a rate limit
error, `UNAVAILABLE` an overloaded error, `UNAUTHENTICATED` an authentication
error, `PERMISSION_DENIED` a permission error, `NOT_FOUND` not found, and
`INVALID_ARGUMENT` an invalid request. Errors the model returns in Messages API
form pass through unchanged.

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

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

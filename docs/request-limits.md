# Timeouts, retries and slow servers

A model server on your own machine or network can take minutes to read a long
Flight before it sends anything back, and tens of minutes to compact one.
Batbox waits for it: no limit below fires on a healthy server that is only
slow, and limits are measured as time without any bytes rather than total
time wherever that is possible.

## Where the settings go

Each key below goes in an endpoint entry under `"endpoints"`, or in a named
entry under `"profiles"`, in `native-client.json`. A setup with a single
`base_url` and no endpoints or profiles reads them from the top level of the
file instead. Durations are milliseconds; `0` means no limit.

```json
{
  "endpoints": {
    "gpu-box": {
      "label": "vLLM on the GPU box",
      "base_url": "http://192.168.1.20:8000",
      "provider": "openai-chat",
      "models": ["qwen3-coder"],
      "context_window": 131072,
      "max_output_tokens": 16384,
      "stream_idle_timeout_ms": 3600000,
      "max_retries": 1
    }
  }
}
```

`/config` and `/providers` keep these keys when they save an endpoint.

## Local endpoints

An endpoint is local when its `base_url` is on this machine or a private
network (localhost, `127.x`, `10.x`, `172.16-31.x`, `192.168.x`, `169.254.x`,
`100.64-127.x`, IPv6 `::1`, `fc00::/7` or `fe80::/10`, a host name without dots,
or one ending in `.local`, `.lan`, `.home.arpa`, `.internal` or `.ts.net`), or
when it came from a local-server preset such as vLLM, llama.cpp, Ollama or
LM Studio. Set `"local": true` or `"local": false` to decide yourself.

A local server usually works on one request at a time and keeps only the last
prompt it read, so Batbox avoids sending it extra requests: it retries at most
twice with short waits, asks once for a compaction summary, and memory recall
picks memories without a model call (`memoryRecallSelector` in
[Configuration](indigenous-configuration.md#memory-recall)). `local` never
changes a time limit: every endpoint gets the same ones.

## Settings

| Key | Default | What it limits |
| --- | --- | --- |
| `local` | detected as above | Retry counts, compaction summary tries and memory recall, as above. No time limit. |
| `stream_idle_timeout_ms` | `21600000` (6 hours) | A streamed request that receives no bytes for this long, including the wait before the first byte, is ended as timed out. `0`: no limit. |
| `request_timeout_ms` | none | Total time for one request, streamed or not. |
| `connect_timeout_ms` | `120000` (2 minutes) | Opening the connection. Must be positive. |
| `nonstreaming_fallback` | `false` | After a stream fails partway, send the whole prompt again without streaming. Off by default: on a slow model the resend repeats the whole wait. |
| `nonstreaming_timeout_ms` | none | Total time for that non-streaming resend, when `nonstreaming_fallback` is on. |
| `max_retries` | `10`, at most `2` for local and non-Anthropic endpoints; a value set here is used as is | Retries after a failed request; every retry sends the whole prompt again. Waits between retries grow from 0.5 seconds, up to 2 seconds for local and non-Anthropic endpoints and 32 seconds otherwise; a `Retry-After` header from the server is used as given. |

A request that reached the server and then timed out (no bytes for
`stream_idle_timeout_ms`, or past `request_timeout_ms`) is not retried: the
error is shown instead, since a resend would only repeat the wait. A connection
that could not be opened within `connect_timeout_ms` is retried like any other
connection failure.

## Requests besides yours

| Request | When | Turn it off |
| --- | --- | --- |
| Flight title | Once per new Flight, alongside your first message; it carries at most the first 2000 bytes of that message. | `"auto_title": false` in `native-client.json` |
| Memory recall | Each of your messages, only with topic files present, and only as a model request when `memoryRecallSelector` picks one. | `"memoryRecallEnabled": false` in `settings.json` |
| Compaction | When the conversation passes `compact_at`, or `/compact`. | `"autoCompactEnabled": false` |
| Memory consolidation | After a turn, only with `"autoDreamEnabled": true`. | leave `autoDreamEnabled` unset |

## Compaction

Compaction first asks for a summary over the conversation the server already
has cached. If that fails for a reason other than a timeout, Batbox asks once
more with the conversation alone; a timeout or a cancelled compaction is not
resent. Both requests ask for the summary with the endpoint's
`max_output_tokens` cap, and the prompt tells the model how long its reply may
be. A summary that still reaches the cap is kept as far as it got: the summary
message says it was cut short and gives the path of the full transcript, and
only a reply with no text counts as a failure. A failed automatic compaction
shows its reason in the status line and in `native.log`. These keys go in `settings.json`:

| Key | Default | What it limits |
| --- | --- | --- |
| `autoCompactFailureLimit` | `3` | Failed automatic compactions during one of your messages before Batbox stops trying until the next one. A timeout ends the tries for that message at once. |
| `compactSummaryAttempts` | `1` for local endpoints, `2` otherwise | Requests for a summary without the cached conversation after the first try fails. |
| `compactRetriesWhenTooLong` | `3` | Retries when the conversation is too long to summarize; each drops the oldest turns, so the server rereads it. |

## Fixed values

| What | Value | Notes |
| --- | --- | --- |
| Stream watchdog in the model layer | off | Not configurable and has no duration of its own; `stream_idle_timeout_ms` covers the same case at the transport. |
| Memory recall selector retries | `2` | Only when recall uses a model request (`memoryRecallSelector`). |

The environment variables `API_TIMEOUT_MS`, `BATBOX_MAX_RETRIES`,
`BATBOX_DISABLE_NONSTREAMING_FALLBACK`, `BATBOX_ENABLE_STREAM_WATCHDOG` and
`BATBOX_STREAM_IDLE_TIMEOUT_MS` are not read; use the keys above.

## Every time limit

Everything in Batbox that can end something you or the model are waiting on.
Limits on a model call, or on work a model drives, are set so that a healthy
slow model is never cut off. Limits that stay short guard quick checks and
probes, never model work.

| What it limits | Where | Default | Setting |
| --- | --- | --- | --- |
| Model request, total (Anthropic, OpenAI-chat, Bedrock, Vertex; main Flight, Bats, compaction, memory recall, Flight title, hooks) | `src/providers/RequestSettings.hpp:10` | none (was 4 hours, none when local) | `request_timeout_ms` |
| Model stream, no bytes received | `src/providers/RequestSettings.hpp:11` | 6 hours (was 2 hours) | `stream_idle_timeout_ms` |
| Model connection opening | `src/providers/RequestSettings.hpp:12` | 2 minutes (was 10 seconds) | `connect_timeout_ms` |
| Non-streaming resend after a failed stream | `src/providers/RequestSettings.cpp:86` | off (was on unless local) | `nonstreaming_fallback` |
| That resend, total | `src/providers/RequestSettings.hpp:13` | none (was 5 minutes, none when local) | `nonstreaming_timeout_ms` |
| Stream watchdog in the model layer | `src/model/Lifecycle.hpp:13` | off, no duration (was off, 90 seconds if enabled) | not configurable |
| Retries of a failed model request | `src/retry/Policy.cpp:9`, `src/application_model/ServicesQuery.cpp:71` | 10, at most 2 for local and non-Anthropic endpoints; a timed-out request is not retried (was retried) | `max_retries` |
| Wait between retries | `src/retry/Policy.cpp:10`, `src/retry/Machine.cpp:122`, `src/application_model/ServicesQuery.cpp:72` | 0.5 seconds doubling, at most 2 seconds (local and non-Anthropic) or 32 seconds | not configurable |
| Compaction and its fallback request | `src/compaction/Summary.cpp` | no limit of its own; the model request limits above | `compactSummaryAttempts`, `autoCompactFailureLimit` |
| Prompt hook (`"type": "prompt"`) | `src/instructions/hooks/runtime/PoetryHookExecutor.cpp:176` | 1 hour (was 30 seconds for the model call inside a 10 minute hook) | the hook's `timeout` |
| Agent hook (`"type": "agent"`) | `src/instructions/hooks/runtime/BatHookExecutor.cpp:242`, `HookPerHook.hpp:10` | 1 hour (was 60 seconds) | the hook's `timeout` |
| Command, HTTP and function hooks | `src/application_hooks/Provider.hpp:56`, `src/instructions/hooks/runtime/HttpHook.cpp:64` | 10 minutes | the hook's `timeout` |
| Async hook | `src/instructions/hooks/runtime/AsyncHookRoster.cpp:34` | 15 seconds | the hook's `asyncTimeout` |
| SessionEnd hooks on `/clear` | `src/application_commands/Commands.cpp:134` | 10 minutes | not configurable |
| TaskOutput wait on a Bat | `src/gadgets/mission_output/MissionOutput.cpp:13` | 1 hour; ends as "still running", not failed | TaskOutput `timeout` |
| TaskOutput wait on a background shell command | `src/gadgets/mission_output/MissionOutput.cpp:15` | 30 seconds | TaskOutput `timeout` |
| TaskOutput longest wait | `src/gadgets/mission_output/MissionOutput.cpp:14` | 24 hours (was 1 hour) | not configurable |
| Wait for a free Bat slot | `src/application_bats/IndigenousBatCore.cpp:288` | no limit | `--max-concurrent-subagents` sets the slots |
| Bash command | `src/application_shell/ApplicationShell.hpp:25` | 2 minutes, at most 10 minutes; unchanged | Bash `timeout` |
| Bash auto-backgrounding | `src/application_shell/ApplicationShell.hpp:26` | off (15 seconds when on) | not configurable |
| MCP server connect | `src/mcp/manager/IndigenousFactory.cpp:119,158` | 30 seconds | `MCP_TIMEOUT` |
| MCP tool call | `src/mcp/manager/IndigenousFactory.cpp:118,159` | about 27.8 hours | `MCP_TOOL_TIMEOUT` |
| MCP other requests (list tools and the like) | `src/mcp/manager/IndigenousFactory.cpp:119`, `src/mcp/remote/RemoteClient.hpp:19` | 60 seconds | not configurable |
| MCP sign-in browser wait | `src/mcp/auth/Flow.cpp:111` | 5 minutes | not configurable |
| MCP `headersHelper`, browser open | `src/mcp/manager/IndigenousFactory.cpp:134`, `src/mcp/auth/IndigenousServices.cpp:126` | 10 seconds | not configurable |
| LSP request, LSP server start | `src/lsp/Manager.cpp:177`, `src/lsp/Manager.hpp:17` | 15 seconds each | not configurable |
| Shell snapshot | `src/shell/SnapshotRun.cpp:86` | 10 seconds | not configurable |
| Cron gate refresh | `src/cron/config/CronConfiguration.hpp:9` | 5 minutes | not configurable; a scheduled turn itself has no limit |
| Ripgrep (Grep, Glob, file search) | `src/filesystem/RipgrepProcess.hpp:12` | 20 seconds; short on purpose | not configurable |
| Web fetch | `src/connectors/Scrapling.hpp:59` | 2 hours | not configurable |
| Web search | `src/gadgets/web/SearchCore.cpp:30`, `src/connectors/Searxng.cpp:168` | 2 minutes, connect 10 seconds | not configurable |
| SearXNG and Scrapling status (`doctor`, `/connectors`) | `src/application_connectors/Status.hpp:61,66,78` | 5, 2 and 30 seconds; short on purpose | not configurable |
| Local-server discovery probe | `src/providers/LocalDiscovery.hpp:31` | 300 milliseconds; short on purpose | not configurable |
| Model list and connection test | `src/providers/WireClients.hpp:30`, `src/application_config/Discovery.cpp:25` | 8 seconds (12 in `/config`), connect at most 5 seconds; short on purpose | not configurable |
| Cloud credentials (GCP token, metadata server, AWS STS, Bedrock model list) | `src/providers/gcp/GoogleAuth.cpp:59,114`, `src/providers/aws/Credentials.cpp:166`, `src/providers/aws/BedrockClient.cpp:217` | 30, 3, 30 and 20 seconds | not configurable |
| `batbox -p "text"` waiting on stdin | `src/application_cli/Adapter.cpp:264,302` | no wait when a prompt argument is given and stdin has nothing ready (was 3 seconds); 3 seconds without a prompt argument | not configurable |
| Clipboard image, message copy | `src/application_media/ClipboardImage.cpp:35`, `src/interactive/navigation/MessageCopy.hpp:8` | 3 and 2 seconds | not configurable |
| Git branch and worktree lookups | `src/application_flights/Flight.cpp:81`, `src/application_flights/RestoreCommand.cpp:69` | 10 seconds | not configurable |

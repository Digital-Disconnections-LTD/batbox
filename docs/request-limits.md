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
prompt it read, so Batbox avoids sending it extra requests: it does not resend
a failed stream without streaming, and memory recall picks memories without a
model call (`memoryRecallSelector` in [Configuration](indigenous-configuration.md#memory-recall)).

## Settings

| Key | Default | What it limits |
| --- | --- | --- |
| `local` | detected as above | Whether the endpoint gets the local defaults in this table. |
| `stream_idle_timeout_ms` | `7200000` (2 hours) | A streamed request that receives no bytes for this long is ended as timed out. `0`: no limit. |
| `request_timeout_ms` | none for local endpoints, `14400000` (4 hours) otherwise | Total time for one request, streamed or not. |
| `connect_timeout_ms` | `10000` (10 seconds) | Opening the connection. Must be positive. |
| `nonstreaming_fallback` | `false` for local endpoints, `true` otherwise | After a stream fails partway, send the whole prompt again without streaming. |
| `nonstreaming_timeout_ms` | none for local endpoints, `300000` (5 minutes) otherwise | Total time for that non-streaming resend. |
| `max_retries` | `10`, at most `2` for local and non-Anthropic endpoints; a value set here is used as is | Retries after a failed request; every retry sends the whole prompt again. Waits between retries grow from 0.5 seconds, up to 2 seconds for local and non-Anthropic endpoints and 32 seconds otherwise. |

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
resent. A failed automatic compaction shows its reason in the status line and
in `native.log`. These keys go in `settings.json`:

| Key | Default | What it limits |
| --- | --- | --- |
| `autoCompactFailureLimit` | `3` | Failed automatic compactions during one of your messages before Batbox stops trying until the next one. A timeout ends the tries for that message at once. |
| `compactSummaryAttempts` | `1` for local endpoints, `2` otherwise | Requests for a summary without the cached conversation after the first try fails. |
| `compactRetriesWhenTooLong` | `3` | Retries when the conversation is too long to summarize; each drops the oldest turns, so the server rereads it. |

## Fixed values

| What | Value | Notes |
| --- | --- | --- |
| Stream watchdog in the model layer | off | Not configurable; `stream_idle_timeout_ms` covers the same case at the transport. |
| Memory recall selector retries | `2` | Only when recall uses a model request (`memoryRecallSelector`). |

The environment variables `API_TIMEOUT_MS`, `BATBOX_MAX_RETRIES`,
`BATBOX_DISABLE_NONSTREAMING_FALLBACK`, `BATBOX_ENABLE_STREAM_WATCHDOG` and
`BATBOX_STREAM_IDLE_TIMEOUT_MS` are not read; use the keys above.

# Endpoints, context, usage and code intelligence

The indigenous terminal now exposes these through `/help`:

| Command | Purpose |
| --- | --- |
| `/context` | Current context, capacity, compaction threshold, output allowance, and estimated category breakdown. |
| `/status` | Flight token/stash usage, current endpoint/model, and indigenous runtime lifetime counters. |
| `/config` | Add, edit and remove endpoints; manage authentication, model discovery and request capabilities. |
| `/model` | Browse endpoint model lists, search them, refresh discovery, enter a model ID, and select a model. |

Context totals use the latest backend input/stash usage when available and identify
new-message estimates separately. Category counts are estimates. Runtime memory
counters describe retained runtime objects and strings, not process RSS or GPU
memory. Local inference prices are not invented.

## Compaction controls

`/context` shows measured context usage, model capacity and the effective
compaction threshold. Its controls toggle automatic compaction and edit the
token threshold. The same edits are available as `/context auto on`,
`/context auto off` and `/context threshold 100000`. `/config` shares that
threshold. Successful changes persist in `native-client.json` and apply on the
next model request, including child Bat requests.

The effective threshold includes the model's reserved summary/output space and
context buffer. It may be lower than the configured threshold, especially after
selecting a smaller model. Environment disable/percentage overrides retain
precedence and the panel reports the effective setting. Turning automatic
compaction off still permits manual `/compact`.

## Endpoint setup

Open `/config`, select **Add endpoint**, and enter a label and base URL. Each
endpoint has its own Messages path, models discovery path, authentication header,
API key or environment variable, custom headers, token limits and capabilities.
Keys and custom headers use a masked input. Configuration stays in the private
indigenous client configuration file; endpoint edits preserve unrelated settings.

**Discover / refresh models** calls the endpoint's models resource (normally
`GET /v1/models`). Model IDs are deduplicated and stored per endpoint. Discovery
supplies names; context limits and capabilities come from the endpoint settings.
An unavailable or empty model list leaves manual model entry available.

`/model` can search a long list and select an endpoint-specific route even when
multiple endpoints serve the same model name. Adding the first endpoint in
`/config` enables this browser immediately. In-flight requests retain their own
configuration snapshot; edits affect subsequent requests. Global request settings
marked **next launch** take effect after restarting the client.

## Indigenous LSP

The `LSP` gadget uses an Indigenous C++ JSON-RPC client and stdio language servers. It
supports definitions, references, hover, document/roost symbols,
implementations and call hierarchy. File Write/Edit notifications feed the same
roost server, and diagnostics become gadget-context attachments.

Installed language servers are detected lazily. For explicit setup, create
`~/.batbox/lsp.json` or `.batbox/lsp.json` in a project:

```json
{
  "servers": {
    "cpp": {
      "command": "clangd",
      "args": ["--background-index"],
      "extensionToLanguage": {
        ".c": "c", ".h": "cpp", ".cpp": "cpp", ".hpp": "cpp"
      }
    }
  }
}
```

Example indigenous gadget arguments (positions start at one):

```json
{"operation":"goToDefinition","filePath":"src/main.cpp","line":12,"character":7}
```

Servers start on demand and close when an idle roost retires or the client
exits. The indigenous client bounds its open-document working set and diagnostic
history. A language server is a separate process with its own indexing memory.

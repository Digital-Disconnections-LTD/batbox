# More Gadgets, more possibilities

MCP servers extend Batbox with capabilities and resources. `/mcp` shows connection
state and actions for reconnecting, enabling, disabling, login, and logout.

User servers load from `~/.batbox/mcp.json`; a project’s `.mcp.json` overlays those
names. An explicit client `mcpServers` object or `--mcp-config PATH` selects a
configuration instead.

```json
{
  "mcpServers": {
    "local": {"command": "my-mcp-server", "args": []},
    "remote": {"type": "http", "url": "https://example.org/mcp"}
  }
}
```

Batbox supports stdio, Streamable HTTP, and SSE connections. Server poetry commands
join the slash-command catalog; resources can join the conversation context.
Configuration strings support `${VARIABLE}` and `${VARIABLE:-default}`.

Remote headers may be static or come from `headersHelper`. That executable receives
`BATBOX_MCP_SERVER_NAME` and `BATBOX_MCP_SERVER_URL`, then returns a JSON object of
string header values. Keep secrets in your private configuration or environment.

OAuth-capable servers can use login/logout from `/mcp`; credentials persist in the
platform credential store. Desktop browser authentication and credential behavior
still need release acceptance on each platform. Enterprise XAA, WebSocket, IDE,
and in-process transports are not part of the current release scope.

For Batbox’s built-in web connections, see [Scrapling and SearXNG](connectors.md).

# Web magic, with excellent company ✨

Batbox connects to two independent open-source projects:

- **[Scrapling](https://github.com/D4Vinci/Scrapling)** provides web fetching,
  extraction, and browser capabilities through its MCP server.
- **[SearXNG](https://github.com/searxng/searxng)** provides metasearch through
  the instance you choose.

Install and configure these services using their own guides. In your private
`native-client.json`, point Batbox at the Scrapling executable and SearXNG instance:

```json
{
  "scrapling_command": "/absolute/path/to/scrapling",
  "searxng_url": "http://127.0.0.1:8888/search"
}
```

`scrapling_command` is an executable path, not a shell command. Batbox starts it
with the `mcp` argument, so it must have Scrapling’s MCP dependencies available.
The current startup expects this helper even when your first request does not
use the web. `scrapling` on PATH is the default.

Set `searxng_url` to the complete search endpoint, including `/search`, rather
than the instance homepage. The homepage can return an HTTP 308 redirect;
Batbox expects a JSON search response from the configured URL.

Use a SearXNG instance with JSON search responses enabled. Search terms go to
that instance; fetching and browser requests go to their target sites. Saved
browser state can contain authentication data, so keep it outside your repository.

Search and browser work support cancellation. A browser helper is a separate
process, and its own website timeouts may differ from Batbox’s connection timeout.

These integrations do not imply affiliation or endorsement. We are grateful for
the work of both communities.

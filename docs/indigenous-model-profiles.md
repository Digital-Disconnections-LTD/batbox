# Choose your model’s next stage

Use `/config` to create endpoints and `/model` to choose a model. For named
routes, `native-client.json` also accepts `profiles`:

```json
{
  "profile": "local",
  "profiles": {
    "local": {
      "model": "your-model-id",
      "base_url": "http://127.0.0.1:8000",
      "messages_path": "/v1/messages",
      "context_window": 200000,
      "max_output_tokens": 8192,
      "auth_header": "x-api-key",
      "api_key_env": "BATBOX_MODEL_KEY",
      "thinking": false,
      "prompt_caching": false
    }
  }
}
```

The token limits are examples: use your backend’s actual capacity. Turn on
thinking and poetry stashing only when the backend supports them. The configuration
key for poetry stashing is `prompt_caching`.

`--profile NAME` or `BATBOX_PROFILE` selects a startup profile. `/model NAME`
changes the route for the next turn; `/model default` returns to the startup route.
`--model`, `--base-url`, `--context-window`, and `--max-output-tokens` override the
selected route for that invocation.

`auth_header` accepts `authorization` for Bearer tokens, `x-api-key` for raw keys,
or `none` when custom `headers` handle authentication. Credentials are checked in
this order: `auth_token`, `api_key_env`, then `api_key_helper`. Named profiles have
their own credentials. Prefer an environment variable or credential helper.

For a complete first connection, follow [Getting started](getting-started.md).

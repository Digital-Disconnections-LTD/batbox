# Your first flight 🦇

You need a Batbox package for your operating system and a model server that
supports **Anthropic Messages streaming**. A server offering only OpenAI chat
completions is not enough for this client.

## Unpack and get comfortable

Keep the package folders together. On Linux and macOS, the executable is
`bin/batbox`; on Windows it is `bin/batbox.exe`. The Windows `bin/` and `etc/`
folders belong next to each other, so the DLLs and certificate bundle stay where
the app expects them.

From the extracted folder, run your installer:

| Platform | Install command |
| --- | --- |
| macOS | `bash gadgets/install-macos.sh` |
| Linux | `bash gadgets/install-linux.sh` |
| Windows PowerShell | `.\gadgets\install-windows.ps1` |

The installer keeps the package together and puts `batbox` on your user PATH.
Open a new terminal on Linux or macOS, then type `batbox` from any project.
No administrator access is needed. Your settings stay in `.batbox` at home.
The same scripts work from a source checkout after its build prerequisites are
installed; they compile Batbox before installing it.

Git, Bash, and ripgrep support the coding gadgets. The current startup also needs
Scrapling and its MCP dependencies available through `scrapling_command` or PATH.
The [platform guides](README.md#your-platform-your-terminal) cover runtime details.
A source checkout needs the separate [developer build reference](BUILDING.md).

## Connect your model

Batbox reads `native-client.json` from your home directory’s `.batbox` folder:

| Platform | Configuration file |
| --- | --- |
| Linux and macOS | `~/.batbox/native-client.json` |
| Windows | `%USERPROFILE%\.batbox\native-client.json` |

Create that folder and file, then adapt this example. Replace the URL and model
ID with values from your server operator, and set the token limits to match your
model. The numbers below are examples, not discovered capabilities.

```json
{
  "profile": "main::your-model-id",
  "endpoints": {
    "main": {
      "label": "My model",
      "base_url": "https://model.example.com",
      "messages_path": "/v1/messages",
      "models_path": "/v1/models",
      "auth_header": "x-api-key",
      "api_key_env": "BATBOX_MODEL_KEY",
      "models": ["your-model-id"],
      "context_window": 200000,
      "max_output_tokens": 8192
    }
  },
  "compact_at": 100000
}
```

Use the server origin for `base_url`; `messages_path` supplies `/v1/messages`.
If your server requires Bearer authentication, use `"auth_header": "authorization"`.
Your key can live in an environment variable, so it stays out of project files.

Linux or macOS:

```sh
export BATBOX_MODEL_KEY='your-api-key'
batbox --cwd /path/to/project
```

Windows PowerShell:

```powershell
$env:BATBOX_MODEL_KEY = 'your-api-key'
batbox --cwd C:\path\to\project
```

These variables apply to the current terminal flight. Once inside, `/config`
lets you edit the endpoint and credentials. `/model` discovers available model
IDs or lets you enter one manually. Enable thinking and poetry stashing only when
your backend supports them.

## Say hello, then do something useful

A little wingbeat, a friendly ghost, and you're home. In the full-screen terminal,
the pair greets you on opening, settles near the upper third, and scrolls along
with your conversation. Scroll back to the top whenever you want to say hello.

Start small: “Tell me what this project does. Don’t edit anything yet.”
Then try: “Find the code responsible for settings and explain how it works.”

Use `/permissions` to choose your permission mode. `/help` lists the commands available in
your running version. `/context` shows your conversation’s context budget,
`/status` shows the current model and usage, and `/exit` closes the app.

Want a quick answer from a script? Use `--print`:

```sh
batbox --print "Explain this project's entry point in three sentences."
```

Headless `--print` runs gadgets automatically by default, including child Bats.
It never waits for an approval dialog. An explicit `--permission-mode` can choose
a more restrictive mode for a particular run. The same flags work in PowerShell.

## If takeoff is bumpy

| Symptom | Check |
| --- | --- |
| Authentication fails | The key, `auth_header`, and environment variable in the terminal launching Batbox. |
| The server reports an unknown model | Its exact wire model ID; refresh `/model` or enter it manually. |
| The request returns 404 | The combination of `base_url` and `messages_path`. |
| A library or DLL is missing | Your platform guide and the package’s dependency inventory. |
| Scrapling cannot start | `scrapling_command`, PATH, and the helper’s Python/browser dependencies. |
| Configuration will not load | Valid JSON: commas between entries, with no trailing commas. |

Next stop: [Everyday Batbox](everyday-use.md).

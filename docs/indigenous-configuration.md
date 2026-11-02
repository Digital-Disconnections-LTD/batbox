# Make your Roost comfortable

Batbox keeps user configuration in `~/.batbox/` on Linux and macOS, or
`%USERPROFILE%\.batbox` on Windows. Your project can add its own `.batbox/` folder.

| File or directory | What belongs there |
| --- | --- |
| `native-client.json` | Endpoints, model profiles, limits, and client options. |
| `settings.json` | Hooks, permissions, and shared preferences. |
| `agents/*.md` | Your Bat definitions, including nested directories. |
| `skills/<name>/SKILL.md` | Reusable skills. |
| `mcp.json` | MCP server connections. |
| `BATBOX.md` and `rules/` | Instructions and project conventions. |
| `system.md` | Optional replacement for the built-in system prompt. |
| `searxng/settings.yml` | Optional local SearXNG settings; Batbox reads its port. `batbox setup web` writes it. |

A Bat definition is Markdown with frontmatter and poetry in its body:

```markdown
---
name: reviewer
description: Review implementation changes
model: inherit
---
Read the relevant code and report concrete findings.
```

Project Bat definitions override user definitions with the same name. Reopen
Batbox after editing definitions. `VISUAL` or `EDITOR` selects your preferred editor.

## Customising the system prompt

The system prompt (Batbox calls it poetry) starts with a fixed head: who the
model is, how to approach tasks, when to ask before acting, and how to write.
The built-in head is [`poetry/system.md`](../poetry/system.md) in this
repository. To replace it, write a copy into your configuration folder and edit it:

```sh
batbox poetry export      # writes ~/.batbox/system.md; --force replaces an existing file
batbox poetry             # prints the head in use and where it comes from
```

When `system.md` exists and has text, it replaces the built-in head entirely.
Batbox checks the file before each request, so edits apply from the next
turn, including after compaction. An empty file, or one holding only
comments, is ignored. `<!-- -->` comments are removed before sending.
`BATBOX_SIMPLE=1` still swaps in its short three-line prompt instead.

The rest of the prompt is built per Flight. Place those sections with
`{{section:NAME}}`: alone on a line it becomes its own paragraph, and inside
a sentence it is inserted as is.

| Placeholder | What it renders |
| --- | --- |
| `{{section:using_your_tools}}` | Guidance for the enabled Gadgets, such as the to-do list. |
| `{{section:session_guidance}}` | Session tips: Bats, skills, denied Gadget calls. |
| `{{section:memory}}` | Your auto-memory instructions and index. |
| `{{section:env_info_simple}}` | Working directory, platform, date and model. |
| `{{section:language}}` | The reply language from settings. |
| `{{section:output_style}}` | The active output style. |
| `{{section:mcp_instructions}}` | Instructions from connected MCP servers. |
| `{{section:scratchpad}}` | The scratchpad folder, when enabled. |
| `{{section:frc}}` | Notes on clearing old Gadget results, when enabled. |
| `{{section:summarize_tool_results}}` | A reminder to note important Gadget results. |
| `{{sections}}` | Every section not placed by name, in the order above. |

`ant_model_override`, `numeric_length_anchors`, `token_budget` and `brief`
are accepted too; they render nothing unless their feature is on. A section
without a placeholder is appended after your text in the order above, so a
file without placeholders still gets every per-Flight section. A section with
no content renders nothing. Any other `{{...}}` stays as written;
`batbox doctor`, `/poetry` and `native.log` name it.

Client options override matching shared settings; CLI and environment overrides
still apply. `--config PATH` selects a client file. `BATBOX_CONFIG_DIR` selects an
explicit configuration root. Keep credentials outside your source repository.

Fresh installations store runtime data under `~/.batbox/state/`. Existing saved
Flights retain their configured location. `state_directory` can select one
explicitly. Vocabulary changes do not relocate your data.

## Bat limits

| Setting | Flag | Environment | Default | What it limits |
| --- | --- | --- | --- | --- |
| `max_subagents` | `--max-subagents N` | `BATBOX_MAX_SUBAGENTS` | 5 | Bats running or waiting at once, counting Bats launched by Bats. |
| `max_concurrent_subagents` | `--max-concurrent-subagents N` | `BATBOX_MAX_CONCURRENT_SUBAGENTS` | 2 | Bats working at once, counting Bats launched by Bats. |
| `max_bat_depth` | `--max-bat-depth N` | `BATBOX_MAX_BAT_DEPTH` | 3 | How deep Bats may launch Bats. |

Depth 1 is a Bat the main Flight launches, depth 2 a Bat that Bat launches, and
depth 3 a grandchild. A Bat below `max_bat_depth` gets the Agent Gadget; a Bat
at it does not, and its poetry says it cannot launch Bats. `0` and `1` both
keep Bats from launching Bats; the main Flight can always launch them.

Set them in `settings.json` or `native-client.json` (for example
`"max_bat_depth": 2`). Later sources win: `settings.json`, then
`native-client.json`, then the environment variable, then the flag. `/config`
shows all three under ⚙ Request settings, saves edits to `native-client.json`,
and applies them on the next launch. A project's `.batbox/` folder does not set
these limits.

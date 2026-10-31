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
| `searxng/settings.yml` | Optional local SearXNG endpoint discovery. |

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

Client options override matching shared settings; CLI and environment overrides
still apply. `--config PATH` selects a client file. `BATBOX_CONFIG_DIR` selects an
explicit configuration root. Keep credentials outside your source repository.

Fresh installations store runtime data under `~/.batbox/state/`. Existing saved
Flights retain their configured location. `state_directory` can select one
explicitly. Vocabulary changes do not relocate your data.

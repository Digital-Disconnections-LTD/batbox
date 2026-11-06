# Make your Roost comfortable

Batbox keeps user configuration in `~/.batbox/` on Linux and macOS, or
`%USERPROFILE%\.batbox` on Windows. Your project can add its own `.batbox/` folder.

| File or directory | What belongs there |
| --- | --- |
| `native-client.json` | Endpoints, model profiles, limits, and client options. |
| `settings.json` | Hooks, permissions, and shared preferences. |
| `agents/*.md` | Your Bat definitions, including nested directories. |
| `skills/<name>/SKILL.md` | Reusable skills. |
| `mcp.json` | MCP server connections; see [MCP servers](connectors.md#mcp-servers). |
| `lsp.json` | Language servers for the LSP Gadget; see [Language servers](#language-servers). |
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

## Hook conditions

A hook in `settings.json` or `native-client.json` can carry an `if` condition,
written like a permission rule. The hook runs only for the Gadget calls the
rule matches; other calls skip it. Conditions work on `PreToolUse`,
`PostToolUse`, `PostToolUseFailure` and `PermissionRequest` hooks. A hook with
`if` on any other event does not run.

```json
{
  "hooks": {
    "PreToolUse": [{
      "matcher": "Bash",
      "hooks": [{"type": "command", "command": "./scripts/check-push.sh",
                 "if": "Bash(git push*)"}]
    }]
  }
}
```

## Hook environment files

`SessionStart`, `Setup`, `CwdChanged` and `FileChanged` command hooks get
`BATBOX_ENV_FILE`, the path of a file for that hook. Lines the hook writes there,
such as `export NODE_ENV=test`, run before every later Bash command in the
Flight. When Batbox itself starts with `BATBOX_ENV_FILE` set, that file runs
first. Hook environment files are not read on Windows.

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
Edits apply when the prompt is next rebuilt: after compaction, `/clear`, a
model switch, or a restart or resume (with `stablePromptPrefix` off, from the
next turn; see below). An empty file, or one holding only comments, is
ignored. `<!-- -->` comments are removed before sending.
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

A section without a placeholder is appended after your text in the order
above, so a file without placeholders still gets every per-Flight section. A
section with no content renders nothing. Any other `{{...}}` stays as written;
`batbox doctor`, `/poetry` and `native.log` name it.

## Keep the prompt prefix stable

A model server keeps the conversation it has already read and only reads the
new part of each request, as long as everything before it is byte for byte
the same. A local server that has to reread a long Flight can take minutes.

The start of every request is the system poetry and a first message holding
your `BATBOX.md` instructions, the date, and the `.batbox/` project notes.
With `stablePromptPrefix` on (the default), Batbox builds both once and keeps
their bytes for the Flight. It rebuilds them only where the server has to
reread anyway: after compaction, after `/clear`, when you switch model or
endpoint, and when you restart or resume.

When one of those files changes in the middle of a Flight, Batbox adds its
new contents (or, for a file over 4 KiB, a diff) to the end of the
conversation once, so the model still sees edits you make in another window.
It skips that note for changes the model itself made with Write or Edit,
since the conversation already holds them. The files followed are the
`BATBOX.md` chain and rules, the auto-memory `MEMORY.md`, and
`.batbox/Makefile`, `00_project.md` and `10_memory.md`. A new topic file in
`.batbox/` joins the index at the next rebuild. `system.md` edits wait for
the next rebuild too.

To rebuild the start of the prompt whenever those files change instead, the
behaviour before this setting existed, set it to false in `settings.json`:

```json
{ "stablePromptPrefix": false }
```

## Memory recall

At the start of each of your turns, Batbox looks through the topic files in
its auto-memory folder and attaches up to five that fit what you asked. Bats
and compaction do not recall. With no topic files there is nothing to do.

How it picks is `memoryRecallSelector` in `settings.json`:

| Value | How memories are picked |
| --- | --- |
| `"auto"` (default) | `"keywords"` when the model's endpoint is local, `"model"` otherwise. |
| `"keywords"` | Words your message shares with each file's name and `description`. No model request. |
| `"model"` | A short request to the same model with your message (its first 2000 bytes) and the list of files. It starts alongside your own request, so on a local server that answers one request at a time it waits ahead of yours and replaces the prompt the server had cached. |

See [Request limits](request-limits.md) for which endpoints count as local.
To turn recall off, set `"memoryRecallEnabled": false`; to turn off auto
memory entirely, set `"autoMemoryEnabled": false`.

```json
{ "memoryRecallSelector": "keywords" }
```

Client options override matching shared settings; CLI and environment overrides
still apply. `--config PATH` selects a client file. `BATBOX_CONFIG_DIR` selects an
explicit configuration root. Keep credentials outside your source repository.

Fresh installations store runtime data under `~/.batbox/state/`. Existing saved
Flights retain their configured location. `state_directory` can select one
explicitly. Vocabulary changes do not relocate your data.

## Automatic compaction

`/context` shows measured context usage, the model's capacity and the effective
compaction threshold. Its controls turn automatic compaction on or off and edit
the token threshold; `/context auto on`, `/context auto off` and
`/context threshold 100000` do the same from the input. `/config` edits the
same threshold. Changes are saved in `native-client.json` as
`autoCompactEnabled` and `compact_at`, and apply from the next model request,
including Bat requests.

The effective threshold leaves room for the summary and a buffer, so it can be
lower than `compact_at`, especially after you select a smaller model. `/context`
then shows both. Two environment variables take precedence over these settings:
`BATBOX_DISABLE_AUTO_COMPACT=1` turns automatic compaction off, and
`BATBOX_AUTOCOMPACT_PCT_OVERRIDE=N` places the threshold at N percent (1 to 100)
of the model's context, less the summary space, instead of using `compact_at`.
It never raises the threshold above the default. `/context` reports the
setting in effect. Turning automatic compaction off still allows `/compact`.
[Request limits](request-limits.md#compaction) covers retries and timeouts.

## Language servers

The LSP Gadget talks to stdio language servers. It supports definitions,
references, hover, document and Roost symbols, implementations and call
hierarchy. Files changed with Write or Edit are sent to the same server, and
its diagnostics reach the model with the next Gadget results.

Batbox uses `clangd`, `typescript-language-server`, and `pyright-langserver` or
`pylsp` when they are on PATH. To add or change servers, create `lsp.json` in
your configuration folder or `.batbox/lsp.json` in a project:

```json
{
  "servers": {
    "clangd": {
      "command": "clangd",
      "args": ["--background-index"],
      "extensionToLanguage": {
        ".c": "c", ".h": "cpp", ".cpp": "cpp", ".hpp": "cpp"
      }
    }
  }
}
```

A project entry replaces a user entry with the same name, and either replaces
a built-in one (`clangd`, `typescript`, `pyright`, `pylsp`). `"disabled": true`
turns a server off. The Gadget takes positions that start at one:

```json
{"operation":"goToDefinition","filePath":"src/main.cpp","line":12,"character":7}
```

Servers start on demand and stop when an idle Roost retires or Batbox exits.
Batbox keeps at most 128 documents open at once. A language server is a separate
process with its own indexing memory.

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

## Settings added for scheduling, cleanup and the shell

These go in `settings.json`.

| Setting | Default | What it does |
| --- | --- | --- |
| `cronEnabled` | `true` | Lets the model schedule prompts with CronCreate, see them with CronList and cancel them with CronDelete. `BATBOX_DISABLE_CRON=1` turns it off for one launch. |
| `cronDurable` | `false` | Lets a scheduled job be saved to `.batbox/scheduled_tasks.json` in the Roost so it survives a restart. Off, every job lasts only for this session and nothing is written to the project. Read at launch. |
| `cleanupPeriodDays` | `30` | After startup Batbox deletes, under its state directory, Flight transcripts and their Gadget results, plans, debug logs, background shell output, file-history backups, Flight environment folders, pastes and image stashes older than this. The current Flight and any Flight another running Batbox holds are never touched; `0` keeps nothing older than now apart from those. |
| `shellSnapshot` | `false` | Once per launch, run your shell's rc file (`~/.zshrc` or `~/.bashrc`) and save its functions, aliases and options; each Bash command then sources that snapshot, so your aliases and functions work there. If the snapshot fails, commands run as they do without it. The file is removed when Batbox exits. Read at launch. |
| `gadgetResultBudget` | `false` | When the Gadget results in one message together pass 200,000 characters, the largest are saved to files and the model gets a preview with the path. Read at launch. |

A scheduled job fires only while Batbox is idle: a job that comes due during a
turn waits for the turn to end. It shows as `Running scheduled task (time):
prompt` and its prompt is queued behind anything you already queued. With
`--print`, jobs fire only while `--input-format stream-json` input is still
open. Recurring jobs expire after 30 days.

## File permission rules and symlinks

A `deny` or `ask` rule for Read, Edit or Write holds for every name a path goes
by: as written, through each symlink, and its real path. A symlink in the Roost
that points at a denied folder is denied too, and searching a folder (Glob or
Grep) counts as reading what is in it, so `Read(/secret/**)` also stops
`Glob` and `Grep` in `/secret` or a link to it. A symlink that leads out of the
Roost does not count as inside it, and a Bash `cd` through such a link is moved
back. `allow` rules still match the path as written.

## Instruction file size

An instruction file (`BATBOX.md`, `BATBOX.local.md`, `.batbox/BATBOX.md` and
the files they include) over 40,000 characters is sent whole with every
request. Batbox never cuts it, but it names it: `/memory` marks it `(large)`
with its size, `batbox doctor` lists it under Instruction files, and the log
records it.

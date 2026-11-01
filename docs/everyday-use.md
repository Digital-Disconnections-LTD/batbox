# Everyday Batbox

A good opening piece of poetry gives Batbox a destination and a little context. “Fix this
error” works better with the error text, the command you ran, and what you expected.

## A few good starting points

- “Explain the request flow from this route to the database.”
- “Find the cause of this failing test and make the smallest useful fix.”
- “Review my changes for regressions before I commit.”
- “Split this investigation among bats and show me what each one finds.”

File and shell gadgets run in your selected project. Use `--cwd PATH` at launch
to choose it. Gadget approvals follow your configured permission mode.

## The handy commands

| Command | What it does |
| --- | --- |
| `/help` | Show the live command catalog. |
| `/config` | Manage endpoints, authentication, and request limits. |
| `/permissions` | Choose the current Flight's permission mode. |
| `/model` | Choose a model for upcoming turns. |
| `/effort` | Show or set the model server's reasoning effort. |
| `/context` | See context usage and adjust automatic compaction. |
| `/status` | Check the current flight, model, and usage. |
| `/bats` | Open bat conversations and edit bat definitions. |
| `/leader` | Return from a child conversation to the main conversation. |
| `/memory` | Browse, edit, and refresh project and bat memory. |
| `/mcp` | Manage connected gadget servers. |
| `/resume` | Pick a saved conversation. |
| `/compact` | Summarize the current conversation. |
| `/monster` | Meet your terminal’s little resident. |
| `/exit` | Close Batbox cleanly. |

## Bring a crew

Use `/effort low`, `/effort medium`, or `/effort high` to send
`output_config.effort` with upcoming Messages requests. `/effort auto` clears
the override and leaves the choice to the server. The selection is saved in
`~/.batbox/settings.json` and can be changed while a turn runs; that turn keeps
its original request. A configured model must support effort for the command to
set a level. This setting does not change Batbox's separate thinking token
budget. `/config` also opens during an active turn.

Open `/bats` to follow a child’s poetry, reasoning, gadgets, and replies. Send a
follow-up right in its conversation. Completed children can resume their existing
history; messages to running children wait for a turn boundary. `/leader`, or Esc
with an empty child input, returns to your main conversation.

Ctrl+B from the leader view can background a foreground child. The mission footer
helps you return to active work. Custom Bat poetry live in
`~/.batbox/agents/` or your project’s `.batbox/agents/`.
Agent progress shows each task's status and latest tool. Foreground agents return
their final results before the leader continues; progress events are display
updates and are excluded from the leader's model requests.

## Choose how you fly

Open `/permissions` (or `/permission`), move with the arrow keys, and press Enter
to select a mode. Esc keeps the current choice. The header shows the active mode;
the selection applies to the current Flight and its launched Bats.

| Mode | Behavior |
| --- | --- |
| `default` | Ask when an action needs approval. |
| `acceptEdits` | Approve ordinary file edits automatically. |
| `bypassPermissions` | Skip all Gadget approval dialogs for the Flight and its Bats. |
| `plan` | Keep the Flight in planning mode. |
| `dontAsk` | Deny actions that would need approval. |

Launch directly with `batbox --yolo` or `batbox --dangerously-skip-permissions`.
Both select `bypassPermissions`, just like `--permission-mode bypassPermissions`.
Explicit deny rules continue to apply. To save a startup preference, set
`permissions.defaultMode` in your client configuration.

## Leave a note for next time

Use `/memory` to inspect the actual instruction and memory files. Ask Batbox to
record a useful project convention or a tricky discovery. Its file-memory workflow
keeps topic files and a `MEMORY.md` index; saved notes remain available after
restart and compaction.

Set `VISUAL` or `EDITOR` if you want bat and memory edits to open in your
favorite editor. Project instructions belong in `BATBOX.md` or `.batbox/rules/`.

## Pick up where you left off

`/resume` opens saved conversations. At launch, `--continue` continues the most
recent conversation for the roost, and `--resume FLIGHT` selects one.
Fresh installations keep runtime data under `~/.batbox/state/`.
On Windows, the same configuration root lives under `%USERPROFILE%\.batbox`.

## Make the terminal feel like home

Fullscreen is the default; `--inline` selects the inline layout. Scrolling above
live output holds your place. **Go to the action** returns to the latest output.
`NO_COLOR=1` selects neutral colors.
Press Esc while a response is running to stop that turn. Batbox adds
“Interrupted the Batbox. What should the model do differently?” to the
transcript. Type your next instruction to continue. At the idle prompt, Esc
retains its input editing behavior.

To copy a range longer than the visible screen, press Ctrl+O for the transcript,
then `c` for its copy view. Drag to select; hold the drag at the top or bottom
edge to scroll, or use the wheel while dragging. Releasing copies the entire
selected range, including offscreen text and spaces. Enter or Ctrl+C copies the
selection again in terminals that support OSC 52. Esc returns to the transcript.
The copy view takes a snapshot when opened; close and reopen it to include new
output.

For Terminal.app over SSH, press Ctrl+O then `y` to show the full transcript in
normal terminal scrollback. Select text there and press Command+C, then Return
to resume Batbox. From the copy view, `y` shows just the selected range (or the
full transcript if nothing is selected). The fullscreen layout stays enabled.

For automation, `--print` returns a response and exits. Add `--output-format json`
for JSON, or `--output-format stream-json --verbose` for streaming events. Run
`--help` for the full flag list. Headless Flights select `bypassPermissions` by
default and never wait for an approval dialog, including child Bats. An explicit
`--permission-mode` overrides that default; actions requiring approval in a
restrictive mode return a denial. A policy disabling bypass remains in effect.

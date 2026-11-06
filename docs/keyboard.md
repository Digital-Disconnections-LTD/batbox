# Keyboard shortcuts

These keys work in the fullscreen layout, which is the default.

## Moving between Bats

The Active Missions list sits under the input box, above the status row. It
lists your running Bats, each sub-Bat (a Bat launched by a Bat) indented under
the Bat that launched it, then background shells. The input box is home: the
main Flight.

The quickest way in needs no modifier: press ↓ in an empty input. Focus moves
into the list onto its first row, `Flight` (the main conversation), and the
header reads `Active Missions (3) · Enter view · x stop · Esc back`.

| Key while the list has focus | What it does |
| --- | --- |
| ↓ / ↑ | Move the `❯` marker. ↑ on the `Flight` row returns to the input. |
| Enter | Open the marked conversation: a Bat, or the main Flight from the `Flight` row. |
| x | Stop the marked Bat, as `/bats` does. |
| Esc | Return to the input without changing what is open. |
| Any other key | Return to the input and type it there. |

↓ only enters the list when the input is empty and you are not stepping back
through earlier poetry with ↑. Clicking a row also opens it.

| Key | What it does |
| --- | --- |
| Alt+↓ | Open the next Bat in the list. From home it opens the top Bat; on the last Bat it stays put. |
| Alt+↑ | Open the previous Bat. From the top Bat it goes home. |
| Alt+← | Go home from any Bat, including a sub-Bat. Only with an empty input; with text, Alt+← and Alt+→ move by word. |
| Esc | In a Bat with an empty input, go back one level (to the parent Bat, or home); it never stops the main Flight's turn from there. In the main Flight, stop a running response; queued messages come back into the input. |
| Ctrl+T | Fold the Active Missions list to one line, or unfold it. |

The Bat you are in is marked with `▸` and shown in bold. While you are in a Bat
the status row is a breadcrumb such as
`Flight › Audit the parser › Check the lexer · Alt+↑↓ move · Alt+← home · Ctrl+T list`,
and the folded list reads `Active Missions (3) · in: Check the lexer`. At home
no row is marked and the status row adds `↓ Bats · Ctrl+T list` while Bats
are running. Switching only changes what you see and where your messages go;
the Bats keep working.

Each row is a Bat's title, what it is doing now (its last Gadget and target),
then its status (`running`, `foreground`, `waiting` for a free slot, `stopped`),
elapsed time, Gadget count and tokens on the right. Tokens are the Bat's latest
context size plus every token it has generated. A Bat that waits for one of the
`max_concurrent_subagents` slots is listed and counted as `waiting` until it
starts. On a narrow terminal the activity goes first, then the Gadget count,
then the elapsed time; the title, status and tokens always stay.

The unfolded list shows up to five rows and scrolls to keep the marked row in
view. On a terminal shorter than 12 rows it folds itself.

## Everyday keys

| Key | What it does |
| --- | --- |
| Ctrl+O | Open the transcript with full Gadget output. |
| Ctrl+R | Search your poetry history. |
| Ctrl+B | Send running foreground work to the background. |
| Ctrl+G | Edit your poetry in `$VISUAL` or `$EDITOR`. |
| Ctrl+C | In a Bat, clear the input, or, when the input is empty, stop the Bat and the sub-Bats it launched. |
| Page Up / Page Down | Scroll the conversation. |
| Ctrl+End | Jump to the latest output. |
| Enter during a turn | Queue the message. It shows at the bottom of the conversation and reaches the model at the next Gadget round, or when the current answer ends; the turn keeps running. |
| ↑ with an empty input | While messages are queued, take the newest one back into the input to edit. Otherwise, recall earlier poetry. |

## Alt keys in your terminal

Batbox reads both forms terminals use for Alt+arrows:

- `ESC [ 1 ; 3 A` (up), `B` (down), `C` (right) and `D` (left). Windows Terminal,
  most Linux terminals and iTerm2 send these.
- An `ESC` prefix before the key. On macOS, Terminal.app sends `ESC b` and
  `ESC f` for Option+← and Option+→ out of the box. For Option+↑ and Option+↓,
  turn on **Use Option as Meta key** in Terminal › Settings › Profiles ›
  Keyboard; Terminal.app then sends `ESC` before the arrow. That setting is off
  by default, which is why plain ↓ into the list works without it.

Batbox also asks the terminal to report when its window gains or loses focus,
and turns that off again when it exits, suspends or opens your editor.

A lone Esc is told apart from an Alt key by timing: a key arriving within 50 ms
of Esc counts as Alt+key, while Esc followed by a later arrow stays Esc then
the arrow. If your desktop or terminal keeps Alt+arrows for itself, Batbox never
sees them; `/bats` and `/leader` still switch Bats.

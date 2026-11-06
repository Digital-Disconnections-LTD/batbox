# Memory plan: talking points

Owner's edit copy, distilled from a conversation on 2026-11-03 reviewing
docs/design/memory-upgrade-plan.md. Ten items in proposed build order, each with
what it is, how it works, its settings, how it is tested, its size, and what is
still open. Edit in place.

## 1. Verbatim compaction with shorter model output

What. The first increment. Batbox does the copying, the model writes less, and
the result is measured before anything else is built. Compaction on the local
inference server can take 30 minutes today, and most of that is the model
writing a long analysis and summary at local decode speeds.

How.
- Batbox copies every user message since the last boundary word for word,
  newest first, plus the paths named in Read, Write and Edit calls. No model
  call.
- The prompt moves to poetry/compact.md with an override in the user config
  directory, loaded the same way as system.md.
- The model writes a facts block and a summary. The analysis block becomes
  optional and capped, off by default for local profiles.
- A second compaction never re-summarizes the first. The old narrative is
  carried forward unchanged and trimmed oldest first when over the cap.

Settings. Analysis block on or off per profile, cap on the analysis block, caps
on the copied user-message block and the paths block.

Test. Wall-clock per compaction on isnotreal before and after. A
post-compaction quiz: what were the constraints, what were the exact numbers.
The phase4 compaction fixture replays saved transcripts offline for the rest,
including byte-for-byte survival of the latest user message and the carry
forward of earlier narrative without re-summarizing.

Size. M.

Open questions. Whether the analysis block should be off for every profile or
only local ones. The default caps on the copied block.

## 2. Prefix cache hygiene

What. Investigation before design. Prompt caches on Anthropic and vLLM are
prefix-only, so the only cache we can keep across compaction is the head. If the
summary fork or the post-compaction head is not a byte-identical prefix, every
compaction and every first turn afterwards pays full prefill, which would be
part of the 30-minute compactions on the local server.

How.
- Check that the summary fork's request is a byte-identical prefix of the
  parent's request.
- Check that the post-compaction head, system prompt plus instructions
  message, is byte-identical to the pre-compaction one.
- Surface cache hits: Anthropic returns cache-read token counts per response,
  vLLM exposes prefix-cache hit rates on its metrics endpoint. One line in the
  debug log and one in doctor.
- Fix whatever the check reveals.
- llama-server is the one backend whose cache-reuse option can shift KV blocks
  and reuse a verbatim tail. Note it, do not design for it yet.

Settings. None for the check.

Test. A fixture that captures the fork request and the first post-compaction
request and compares prefixes. A live run on isnotreal reading the vLLM
metrics before and after a compaction.

Size. S for the check. Whatever it reveals is sized separately.

Open questions. Whether cache statistics belong in the status row as well as
the debug log.

## 3. FlightLog gadget

What. A read-only window onto transcripts that already exist. The model can
read its own earlier log, a child Bat's log, or another Flight of the same
Roost, in bounded ranges, across compaction boundaries. It replaces the
invitation to Read a multi-megabyte JSONL.

How.
- Range mode: entries from..to, one rendered line each, capped at 200 entries
  or 32 KiB.
- Find mode: up to 20 entry numbers whose text contains a string, no bodies.
- Target: the current Flight by default, a child Bat by id, or another Flight
  of this Roost by slug. Anything outside the Flight store is refused.
- Saved Gadget output renders as "[saved at path]" instead of its body.
- Reads past the last compaction boundary, which is the point.
- Replaces the "read the full transcript at" line in the summary message and
  the "tail the JSONL" suggestion in the Bat launch text.

Settings. Entry and byte caps.

Test. Ranges across a boundary, caps, a child Bat's log, another Flight of the
Roost, refusal of a path outside the store, rendering of a huge saved result as
a pointer.

Size. S to M.

Open questions. Whether a Bat may read its parent's log or only its own and its
children's.

## 4. State-tracked rows instead of three card types

What. A decision log and a fact log in one row shape, each row carrying a
state. Only live rows are injected after compaction, so the stored file grows
and the injected part does not. A rejected approach is a cancelled decision, so
that card type goes away.

How.
- Row shape: id, kind, state, text, why or witness, by, time.
- Decision states: proposed, confirmed, superseded, cancelled. Fact states:
  established, contradicted, unknown, retracted.
- Files stay human-readable under the Flight directory: an intent card, a
  decisions file, a facts file, a pointer. Append-only. A state change appends
  a new line that references the old id.
- Written through a Card gadget that enforces transitions and witnesses and
  stamps time and author. An established fact needs a witness; a contradicted
  fact names both sources; a retracted row keeps the old row and adds the
  disproof.
- Re-injected after compaction and on resume, live rows only, newest first,
  with the file path for history.
- Batbox fills the pointer's Waiting-on field from Mission state; the model
  never writes that field.

Settings. Injection caps per file and in total. Whether injection also happens
after every card change.

Test. Allowed and refused transitions, witness required, retraction keeps the
row, write-once intent with amendments, injection of live rows only, resume,
no directory leaves the messages unchanged, non-git cwd, Bat calls land in the
Bat's own directory.

Size. M, after item 3 so the model can read the history it is not shown.

Open questions. Whether old Batbox's notepad gadget argues for one free-form
card beside the typed rows, or instead of them. Whether some local models will
ever call Card, and what the fallback is when they do not. Where the files
live, which is item 10.

## 5. Housekeeping nudge

What. When history gets large, Batbox says so once and the model asks the
user, in words close to the owner's: "My history files are getting large,
would now be a good time to launch a bat and do a review with you?"

How.
- Thresholds: live rows above a count, card files above a size, transcript
  above a size. Crossing one injects a one-time note at the tail: history is
  large, suggest a review with the user, ask before doing it.
- If the user agrees, the model launches a review Bat with FlightLog and Card.
- The Bat proposes: decisions to mark superseded, facts to retract, a pointer
  rewrite. It returns proposals only.
- The main thread asks the user about anything it is unsure of and applies the
  rest.
- Nothing is deleted. Rows change state, so trimming only changes what gets
  injected.

Settings. Every threshold. A snooze, once per threshold crossing.

Test. Nudge fires once per crossing, snooze holds, the review Bat returns
proposals and writes nothing, applied proposals change state and never delete.

Size. S once items 3 and 4 exist.

Open questions. The default thresholds. Whether the nudge should also appear in
the TUI as a badge rather than only in the model's context.

## 6. Reorientation turn

What. A fixed first turn after compaction or resume, assembled by Batbox with
no model call. The model reads where things stand before it changes anything.

How.
- Batbox appends one block at the tail: Bats running or finished since the
  boundary, branch and a short git status in a git Roost, live rows from item
  4, and user messages newer than the pointer.
- The model is asked for a one-line status before acting: what was in
  progress, the next step, what it is waiting on.
- On resume the block opens with "This Flight was resumed."
- The tool-denial gate, which refuses non-read-only Gadgets until the status
  line is posted, ships off by default. Prompt only first, gate opt-in.

Settings. Gate on or off, off by default. Line cap on the git status.

Test. Bats listed, git omitted outside a repo, newer-than-pointer selection,
resume path, the gate when enabled denies only in the first response.

Size. M, after item 4 so the block has cards to show.

Open questions. Gate off by default, the reverse of the upgrade plan's
recommendation, because a slow local model stuck in a denial loop is painful.
Whether the status line should also be written to the pointer.

## 7. Waiting on Bats

What. Prompt fixes so the model knows a launched Bat comes back, and waits when
it should. The rule: if next tasks aren't ready until the agent or agents come
back it should wait; if it has independent work, it does that first.

How.
- The launch text says the result arrives as a notification and names the
  blocking wait that TaskOutput already has, instead of suggesting a tail on
  the raw JSONL.
- The Bat notes say: if the next step depends on a Bat, block on it and do
  nothing else; if there is independent work, do that first, then block.
- TaskOutput gains a tail of rendered lines, default 15, so progress checks are
  bounded.

Settings. Default and maximum tail length.

Test. Launch text and Bat notes contain the new wording; tail over one huge
Gadget result stays bounded; a fixture Flight where the model blocks instead of
re-launching.

Size. S.

Open questions. Whether an engine-level wait, where ending a turn with Bats
running parks the turn and wakes the model on completion, is wanted later.

## 8. Timers for slow backends

What. Every timer and retry counter in the request path assumes cloud speeds.
A 30-minute compaction on the local inference server must not trip any of them.
Inventory first, then design.

How.
- Inventory every timeout and retry counter a slow compaction or a slow Bat
  could hit: HTTP read timeouts, the three-failure compaction stop, per-request
  limits, the proposed quiet-Bat notice.
- Then: every timer a setting; stall detection counts bytes streamed rather
  than wall-clock time; a per-profile slow flag scales the defaults; a timeout
  is never counted as a compaction failure.
- The quiet-Bat notice stays a notice. No automatic stop.

Settings. All of them, per profile where it matters.

Test. A fake server that streams slowly past each old default without a
failure; a compaction that times out once does not count toward the failure
limit; the quiet notice appears and clears on a simulated stall.

Size. S for the inventory, M for the rest.

Open questions. The default values for a profile marked slow. Whether the slow
flag is set by hand or inferred from measured decode speed.

## 9. Byte caps

What. Every cap in the upgrade plan becomes a setting with roomier defaults,
and nothing truncates silently.

How.
- Card injection caps, Bat return cap, and instruction file caps all become
  settings.
- Instruction files warn and never truncate silently. If truncation happens at
  all it shows in the TUI.
- Folded into whichever item owns the cap.

Settings. Each cap.

Test. Warning shown at the threshold, truncation visible when enabled,
settings round-trip.

Size. Folded into the owning items.

Open questions. The new defaults. Whether instruction files should ever be
truncated or only warned about.

## 10. File structure

What. Decided before item 4 creates new directories. Today everything sits
under the state directory keyed by a sanitized cwd:

```text
~/.batbox/state/projects/<sanitized-cwd>/
  <flightId>.jsonl
  <flightId>/subagents/agent-<id>.jsonl
  <flightId>/tool-results/
  memory/MEMORY.md + topic files
```

Items 3 to 5 add cards per Flight, a result file and cards per Bat, and a
proposals directory under memory. The Makefile and the project notes do not
live there; they sit in the Roost's own .batbox directory and travel with the
project.

How.
- Pick the layout, then migrate once. 0.1.x is the cheap time.

Settings. None beyond the state directory location.

Test. Migration of an existing state tree, old paths still resolve during the
transition.

Size. S to M depending on the migration.

Open questions. Three separate choices: the top-level name under the config
directory; a readable project key instead of the sanitized path; and whether
project-side files such as the Makefile and project notes stay in the Roost or
also appear under state.

## Build order

Items 1, 2, 7 and the inventory in item 8 start right away, each proving out on
its own. Items 3 to 5 are one chain: FlightLog, then state-tracked rows, then
the housekeeping nudge. Item 6 waits until item 4 exists so the block has cards
to show. Item 10 is decided before item 4 creates directories. Item 9 is folded
into whichever item owns each cap.

The upgrade plan was checked against 92ffb8e; main is now 4d96ca5, with nested
Bats and the system prompt file already merged.

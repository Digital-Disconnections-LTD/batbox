# Compaction plan

Status: proposal for review, 2026-11-05, second draft. No code has changed.
Compaction only. Cards, FlightLog, memory lifecycle and the project directory
move stay in `memory-talking-points.md`.

What changed from the first draft: the first draft said caches are prefix-only
and put the summary before the tail. That was wrong for an important family of
servers. Research on 2026-11-05 (saved next to the repo as
`audit/kv-cache-reuse-research.md`) confirmed that llama.cpp and servers built
on it can reuse a kept tail, but only in the layout the owner proposed: tail
directly after the unchanged head, summary after it. This draft uses that
layout everywhere.

Words: Flight = session, Bat = sub-agent, Gadget = tool, poetry = prompt.

## 1. The idea

Keep the last 10,000 tokens of the conversation word for word, directly after
the fixed first turn, so a server that can shift its cache reuses them without
re-reading. Re-read `BATBOX.md` at the fixed first turn. Put the summary of
everything older after the tail.

## 2. What servers can reuse

Findings are from the research report, which read server source and docs. They
have not been tested against a live server here.

### 2.1 Two kinds of server

**Shifting servers.** After the shared prefix, they look for a later chunk of
the old prompt that matches the new prompt at its current position, and move
that chunk's cached entries into place. They can skip text that was removed.
They cannot skip text that was inserted. The llama.cpp maintainer's wording:
"we stop reusing chunks at the first token that cannot be reused".

**Prefix-only servers.** Each cached block is keyed by everything before it.
Remove the middle and every later block misses.

| Server | Kind | How it is turned on |
| --- | --- | --- |
| llama.cpp server | Shifting | `--cache-reuse N`, off by default, or `n_cache_reuse` per request |
| KoboldCpp | Shifting | ContextShift, on by default |
| LocalAI | Shifting | `cache_reuse` defaults to 256 on its llama.cpp backend |
| Jan, text-generation-webui | Shifting | A setting, off by default |
| Ollama | Prefix-only in practice | The flag was not found being passed |
| vLLM, stock | Prefix-only | Nothing to turn on |
| SGLang, TGI, ExLlama, MLX-LM, mistral.rs | Prefix-only | |
| Anthropic, OpenAI, Gemini, DeepSeek, Groq, Fireworks | Prefix-only | |

There is a research plugin that adds this to vLLM. It is pinned to an old vLLM
version and was measured on different hardware, so it is not part of this plan.

### 2.2 What that means for the layout

```text
Before compaction    [system][head][ older messages ........ ][ tail 10k ]
Summary request      [system][head][ older messages ........ ][ tail 10k ][instruction]
After compaction     [system][head][ tail 10k ][note: your messages, summary, status]
```

Tokens the server reads fresh, for a 100,000 token conversation. Estimates
from the layout, not measurements.

| Server kind | Summary request | First request after | Total |
| --- | --- | --- | --- |
| Shifting | about 1,000 | about 6,000 (the note only) | about 7,000 |
| Prefix-only | about 1,000 | about 16,000 (tail and note) | about 17,000 |

On a prefix-only server the tail-first layout costs exactly what the old
summary-first layout cost, so there is no reason to keep two layouts. Tail
first is the single layout.

### 2.3 Rules the trick depends on

1. **The summary request must include the tail.** On a one-slot llama.cpp
   server, a request that stops before the tail makes the server drop the
   tail's cached entries. Sending the whole conversation plus an instruction
   extends the cached prompt and costs nothing extra.
2. **The tail must be sent exactly as it was last sent.** Reuse matches tokens.
   Any token inserted before or inside the tail stops reuse for everything
   after it. So no rewriting, clipping or re-rendering of tail messages, and
   thinking text handled the same way as in the previous request.
3. **Nothing else may use the server in between.** A memory-recall query or a
   title query between the summary request and the next request replaces what
   a one-slot server holds. This is also true in normal use, which is one more
   reason those side queries hurt on a local server.
4. **The first tail message must be valid directly after the head.** The cut
   lands on a message boundary and never splits a Gadget call from its result.

### 2.4 Limits to know about

- **Reuse is approximate.** The tail's cached entries were computed while the
  removed text was still visible. Only their positions are corrected. KoboldCpp
  describes the result as different but equally coherent. Nobody has published
  a number for llama.cpp.
- **Some models cannot shift.** llama.cpp turns reuse off for models with
  multi-dimensional position encodings, which the report says includes several
  Qwen families, and for multimodal setups. Sliding-window models need an
  extra flag. This must be checked for the exact model in use.
- **Your server is stock vLLM.** It gets the 17,000 token row above, not the
  7,000 one. The trick starts paying only on a shifting server. The larger
  savings on vLLM are in section 3.

## 3. Where the time probably goes today

Not measured. From reading the code; step 0 exists to check it.

- The summary may run to 20,000 output tokens
  (`src/services/compact/autoCompact.hpp:10`). The prompt asks for an analysis
  block, nine sections, full code snippets and every user message. Writing is
  far slower than reading on a local model.
- The first attempt shares the conversation's prompt. If it returns no text or
  throws, the error is swallowed (`src/compaction/Summary.cpp:18-20`) and a
  second kind of call runs.
- That second call uses a different system prompt and Gadget list
  (`Summary.cpp:46-48`). Nothing is cached for it on any server, so the whole
  conversation is read again. It can run twice.
- The sequence can repeat up to three times in one submission.

## 4. The context after compaction

| # | Block | Written by | Size | Notes |
| --- | --- | --- | --- | --- |
| 1 | System prompt | Batbox | unchanged | Same bytes, stays cached |
| 2 | Fixed turn: `BATBOX.md`, instruction files, date, recovery packet | Batbox | as today | Already exists (`src/application_engine/Defaults.cpp:185`). See 4.1 |
| 3 | Tail: the last 10,000 tokens, word for word | nobody, kept | 10,000 tokens | Whole Gadget rounds only, byte-identical to the last request |
| 4 | Compaction note | Batbox and model | about 6,000 tokens | One message, described below |

The compaction note, as the model sees it:

```text
Earlier parts of this Flight were removed to free context. The messages above
are the most recent ones, unchanged. This note is context, not a new request.
Continue the work shown above.

## Your messages so far, word for word (newest first)
- 15:06 "use the Q3 numbers for site C, not Q2"
- 14:03 "keep it under 900 words"
...

## Paths read or changed
~/notes/budget.xlsx, summary.md, notes/site-c.md

## Summary of what was removed
### Earlier (from the 13:10 compaction)
...
### Since then
Decisions: ...
Rejected: ...
Work done: ...
Open problems: ...
Facts: ...

## Right now
Bats running: research-sources (4m). Messages queued: none.
The full transcript is <path>.
```

The note is the last thing the model reads before it continues, so the
constraints and decisions are fresh. The risk is that the model answers the
note instead of continuing. The first line is there to prevent that, and the
quiz in section 8 checks it.

### 4.1 The fixed turn and the cache

Block 2 sits before the tail, so it decides whether the tail can be reused.

- **Head unchanged.** If `BATBOX.md` and the recovery files have the same
  content as at the last request, block 2 is byte-identical. A shifting server
  reuses the tail. This is the normal case once the head is kept stable for
  the life of a Flight, which is work already in flight.
- **Head changed.** New text in block 2 is an insertion before the tail, and
  shifting servers stop at the first inserted token. The tail is then read
  again, once, about 10,000 tokens. Prefix-only servers read it again in
  either case.

Proposal: always re-read the files at compaction and use the fresh content in
block 2. Correct instructions matter more than one 10,000 token re-read, and
when nothing changed the bytes are the same and the reuse happens anyway. The
alternative, keeping the old head and putting changed files in the note, keeps
the reuse but leaves stale instructions at the top of the context.

## 5. How the summary gets written

### 5.1 The request

The whole conversation as it stands, plus one instruction message. The
instruction names the first kept message and tells the model to summarize only
what comes before it. This is today's fork with a new instruction, not the
dormant head-only path, because of rule 1 in section 2.3. The bookkeeping from
the dormant path (`src/compaction/Partial.cpp`, `src/flights/Relink.cpp`) is
reused for what is kept and for resume.

### 5.2 The prompt

Moves to a file, `poetry/compact.md`, with an override in the user config
directory, loaded like `system.md`. It is short because Batbox does the
copying:

- No analysis block by default. A setting turns it back on.
- No "all user messages" section. Batbox copies those.
- No "current work" or "next step" sections. The tail holds them.
- No full code snippets. Paths are copied and the files are on disk.
- Five sections: Decisions, Rejected, Work done, Open problems, Facts. Facts
  are exact values copied as they appeared.
- A code paragraph only in a git project or after file edits.
- Output cap of 4,000 tokens, down from 20,000. A setting.
- Thinking off for this one call. A setting.

Until decision rows exist, Decisions and Rejected are the decision log.
Because parts are never rewritten, they accumulate instead of blurring.

### 5.3 The second and later compactions

The previous note is part of what gets removed. Batbox carries its user
messages, paths and summary parts into the new note unchanged. The model
summarizes only the messages between the old tail's start and the new tail's
start, and is told not to restate the earlier summary. When all parts together
pass a cap (8,000 tokens, a setting), the oldest part is dropped and a line
says so with the transcript path.

### 5.4 When the summary fails

- Retry once in the same shape, so the cache still applies. The
  different-prompt call is removed.
- Slow is not failed. Only inactivity counts, never wall-clock time.
- If the retry fails, compact without a model: tail, copied user messages,
  paths, and a line saying the summary is missing and where the transcript is.
- Every failure shows a notice with the reason.

### 5.5 Optional: reuse the summary text too

On a shifting server the generated summary sits in the cache right after the
instruction. If the next request carries that summary as the assistant turn it
was, directly after the tail and before Batbox's note, the server can reuse it
as well, and only the short note is read fresh. This needs the text to be
re-sent exactly as generated. It is a later experiment, not part of the first
build.

## 6. Tail rules

- Budget is `compactKeepRecentTokens`, default 10,000. Zero turns the tail off.
- Walk back from the newest message, taking whole rounds.
- The latest user message is always inside the tail.
- Tail messages are sent exactly as before. If the newest round alone is over
  budget, it is kept whole on a shifting server. On a prefix-only server its
  largest Gadget results may be clipped with the saved-output path, since
  nothing is lost there by changing it.
- Passes that edit old messages, such as clearing old Gadget results, skip the
  tail.
- Today Batbox re-attaches the five most recently read files, up to 50,000
  tokens. With a tail these are mostly duplicates. Proposal: re-attach only
  files read outside the tail, capped at 10,000 tokens, inside the note.
- A Bat that compacts follows the same rules. Its Mission brief takes the place
  of user messages.
- Resume rebuilds the same blocks.

## 7. What this takes from the memory plan

| Memory plan item | Here |
| --- | --- |
| D4 prompt as a file | Yes, compaction prompt only |
| D5 verbatim tail | Yes, fixed token budget, placed first |
| D6 what is copied word for word | Yes: Batbox copies user messages and paths, the model lists facts |
| D7 one template for code and non-code | Yes |
| No summary of a summary | Yes |
| Fixed first turn | Already exists, reused |
| Flight cards and decision rows | Later. They go into the note |
| First-turn gate | Not included |
| FlightLog | Later. The note keeps the transcript path until then |

## 8. Steps

Each step ships alone and is measured before the next.

| Step | What | How we know it helped |
| --- | --- | --- |
| 0 | Measure. Per compaction: which path ran, tokens read, tokens the server reported as cached, tokens written, time to first token, total time. Shown in the compaction notice. | Real numbers for one compaction on the local server |
| 1 | Short summary: prompt file, 4,000 token cap, analysis off, thinking off | Total time drops; the quiz still passes |
| 2 | Safe failure: same-shape retry, model-free fallback, visible notice | A forced failure costs one short extra call, not several full reads |
| 3 | Tail first: keep the last 10,000 tokens after the head, note after it; summary request includes the tail; resume tested | The model continues mid-task; on a llama.cpp test server the log shows the tail chunk reused |
| 4 | Copied user messages and paths in the note | Latest user message survives byte for byte |
| 5 | No re-summarizing: parts carried forward with the cap | An exact value from before the first compaction survives the third |
| 6 | Send `n_cache_reuse` on requests to llama.cpp endpoints, and document the server flags | Reuse works without the user changing server settings |

The quiz is a fixed set of questions asked after compaction on a saved
transcript: what were the constraints, what was rejected and why, what are
these three exact values, and does the model continue the task instead of
answering the note. The compaction fixture under `tests/phase4` replays saved
transcripts, so most checks run without a model. Step 3's reuse check needs a
llama.cpp server with a small model, which can run on the Mac.

## 9. Settings

| Setting | Default | Meaning |
| --- | --- | --- |
| `compactKeepRecentTokens` | 10000 | Tail size. 0 turns the tail off |
| `compactTailFirst` | true | false puts the summary before the tail, in time order |
| `compactSummaryMaxTokens` | 4000 | Cap on the new summary part |
| `compactSummaryHistoryTokens` | 8000 | Cap on all summary parts together |
| `compactAnalysis` | false | Ask for an analysis block |
| `compactThinking` | false | Allow thinking during the summary call |
| `compactCarryUserBytes` | 6144 | Cap on copied user messages |
| `compactRestoreFilesTokens` | 10000 | Cap on re-attached files |

Names are placeholders in the existing settings style.

## 10. Work in flight that this builds on

- The per-turn implementer is making the head message stable for the life of
  a Flight, making compaction failures visible and making timeouts
  configurable.
- The same implementer is making memory recall cheaper. Rule 3 in section 2.3
  needs side queries kept away from the compaction window.
- The Gadget implementer has fixed lost output, which makes the tail more
  useful.

Nothing here should start before those branches are merged.

## 11. Questions for you

1. Which server do you want this tuned for? On stock vLLM the tail is re-read
   and the win is the short summary and the safe failure path. The shifting
   trick needs llama.cpp or KoboldCpp, and a model whose position encoding
   allows shifting.
2. Tail size: a fixed 10,000 tokens, or should it scale with the window?
3. Is a 4,000 token summary cap acceptable, given that your messages, the
   paths and the tail are kept exactly?
4. Thinking off during the summary call?
5. If the summary fails twice, is a model-free compaction acceptable, or
   should the Flight stop and ask you?
6. Trim the re-attached files as described in section 6?
7. Does your server report cached token counts in its usage? Step 0 is much
   more useful with it.

## 12. Stashed code (2026-11-05)

Compaction is undecided, so the half-connected "keep one side of a cut" code
was taken out of main in the cleanup. It is all in git history.

- Last present at commit `f0a4f02` on `cleanup/ai-slop`.
- Removed by commit `7e00e2d` on `cleanup/flights-memory`.
- Files: `src/compaction/Partial.cpp`, `src/compaction/poetryEntries/Partial.cpp`,
  `src/compaction/poetryEntries/Prefix.cpp`.
- Functions and fields: `partialCompactConversation`, `getPartialCompactPoetry`,
  `annotateBoundaryWithPreservedSegment`, `poetryEntries::partial` and `prefix`,
  the `recentMessagesPreserved` parameter of `getCompactUserSummaryMessage`,
  `applyPreservedSegmentRelinks`, `hasPreservedSegment` in the scan and load
  path, the preserved-tail record in `query_engine/LifecycleStream.cpp`, and
  the `preserved_segment` field in `application_cli/LocalOutput.cpp`.
- To restore: `git show f0a4f02:<path>` for each file, or revert `7e00e2d`.

What it did not have: a caller, and anything that chooses where to cut. Section
6 of this plan describes the cut rule. Section 5.1 explains why the summary
request should still carry the whole conversation.

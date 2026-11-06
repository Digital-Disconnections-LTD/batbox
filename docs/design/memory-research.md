# syntheticscars.org: what it says about agent memory and context compaction

Prepared 2026-11-02 for the Batbox memory design. The page text was saved locally during research (one file per URL, named from the path; `articles_*`, `papers_*`, `theory_chapter-*`, `gist_*`, `registry_all_scars.md`, third-party repo files prefixed by owner).

## How to read the citations

Every claim carries a bracketed source tag. The tag table is at the end of this file (section 7) with full URLs. Where a number is the site's own claim and not independently checked, I say "claims". Dates: the site is a 2026 snapshot (latest article text mentions 2026-10-31); several counters differ between pages (see section 5).

---

## 1. What the site is

A single-author research and publishing site by Randal L. Schwartz (long-time Perl author and instructor). It documents a personal agentic-coding workflow ("Synthetic Scar Architecture") built for Google Antigravity (Gemini and Claude models) and applied to Dart/Flutter repos: his open-source `BlocSignal` framework and an unnamed commercial industrial monorepo [HOME] [ART-PAIN] [P-EMP]. The home page presents it as "converting organic runtime failures... into high-salience cognitive antibodies for autonomous AI coding systems" with six sections: Registry (scar catalogue), Articles (13 essays, the DEV.to series syndicated), Papers (a 13-chapter "preprint"), Theory (12 chapters of "formal foundations"), Starter Kit (one `WORKFLOW.mk` file) and Submit (a scar-drafting form) [HOME] [KIT] [SUBMIT].

The memory-relevant core is small and concrete: (a) a Unix `init.d`-style directory of numbered state files that an agent re-reads after context compaction, (b) forking heavy work into short-lived sub-agents so the main context never fills, (c) a catalogue of failure records ("scars") that is loaded by routing rather than all at once, with a lifecycle for retiring them. Most of the rest (speech-act logic, energy-landscape physics, Makefile gating of git commit/push/land) is workflow governance for pull-request tickets and only indirectly about memory.

Scale of the corpus I fetched: 13 articles, 13 paper chapters, 12 theory chapters (about 3 MB of text, roughly 330 numbered sections), the 577-entry registry (extracted from the site's compiled JS), the author's public gist of his live workflow files, plus one hop to third-party repos [GIST] [BLUR] [MINER].

Retrieval notes. `deuseek fetch --backend fetcher` returned empty for every page (the site is a Flutter/Dart web app, `main.dart.js`); `--backend stealthy --solve-cloudflare` worked for all pages (a few logged a spurious "captcha_suspected" flag but returned full content). There is no `sitemap.xml`, `robots.txt` or RSS feed (404 route-not-found). Two deviations from your retrieval rules, both on the same public domain: I used plain `curl` on `/`, `/sitemap.xml` (to confirm the SPA fallback) and `/main.dart.js` (7 MB) to read the link and script tags and to extract the registry, because the registry page only renders one entry in text form. The gist and the third-party repos were read with read-only `gh api`. `github.com/RandalSchwartz/Scars` (the registry's source repo, linked from the submit page) and `vinniefalco/scars` return 404 (private or removed).

---

## 2. Content map

Home and section indexes
- `/` home: stats banner (0.0% repeat regression across 202 tickets, 529 scars, 94.2% "attention dividend", 147 "verified deflections") and links to the six sections [HOME].
- `/articles`, `/papers`, `/theory`, `/registry`, `/starter-kit`, `/submit`: indexes [HOME].
- `/registry`: scar explorer. Text render shows only SCAR-A2UI-01; the full data (577 records) is embedded in `/main.dart.js` and is saved as `pages/registry_all_scars.md` [REG].
- `/starter-kit`: a single `WORKFLOW.mk` (Makefile DAG: finish <- land <- ready-to-land <- bot-quiescence <- push <- commit ... with `.state/*-approved.token` human-gate files) [KIT].
- `/submit`: form that emits a 4-section Markdown scar (Wound, Trap, Circuit Breaker, Permanent Reflex) plus Category, Visibility, Workflow Phase fields [SUBMIT].

Articles (all syndicated from DEV.to; all read)
- Compaction and context (highest value): "Surviving the 200k-Token Lobotomy" (init.d, Memento, fork/wait) [ART-COMPACT]; "What LLMs Know That You Don't Know They Know" (the 1,680-trial benchmark, including a compaction test) [ART-GRAVITY]; "926 Steps, Two Lobotomies, and Zero Nudges" (field tests, vendor swap mid-run, small-model failure) [ART-FLIGHT].
- Memory lifecycle and scale: "The Best Prompt Rule Is the One You Delete" (insmod-style scar modules, graduating rules to linters, semantic blur, 280 KB scar bloat) [ART-SENSOR]; "Giving AI Pain" (4-part scar format, transcript mining, Rapid-Regret git miner, Six-Pillar critic) [ART-PAIN]; "All That Glisters Is Softmax" (nine independent replicators, tiering of weights vs disk) [ART-GLISTERS].
- Workflow governance, little memory content: Makefile-not-checklist [ART-MAKE]; Flores/Winograd "committed speech" [ART-FLORES] [ART-REQ]; Socratic nudging and its physics [ART-NUDGE] [ART-PHYS]; layperson intro and the "3 AM" essay [ART-LAY] [ART-3AM].

Papers (13 chapters; read: abstract, introduction, taxonomy, closed-loop architecture, empirical methodology, discussion headings and sections 8.9/8.14, related-work sections 7.1.3 to 7.1.4 and 7.3, further-study 9.4 to 9.6, conclusion headings)
- `/papers/closed-loop-multi-agent-architecture`: 21-step protocol, "gates gather, scars distribute", cross-ticket momentum projector, scar apoptosis, five sensor-lifecycle invariants [P-ARCH].
- `/papers/empirical-methodology-metrics`: metrics, cohorts, dual public/private registry, deflection telemetry, the 7-suite benchmark [P-EMP].
- `/papers/related-work`: scar invalidation and "scar GC", mnemonic anchors, apprentice mode [P-REL].
- `/papers/further-study-...`: replication protocol; lists the control experiments that have not been run [P-FURTHER].
- `/papers/case-studies` (643 KB, 155 Dart/Flutter ticket write-ups): headings only, not read in full [P-CASES].

Theory (12 chapters, 330 sections; the chapter titles on the index do not match the contents, e.g. Chapter 4 "Context Compaction, Memento & Persistence" is actually sections 91 to 120 on the Iceberg architecture, review regimes, and training-data epistemology; the compaction/Memento material lives in Ch. 8 sections 215 to 225 and 231, Ch. 9 sections 258 to 266, Ch. 11 section 291, Ch. 12 sections 320, 325, 338.11, 343). Sections I read are named in the tags in section 7.

Gist (the author's live workflow, compiled prompt files): `SKILL.md`, `GLOBAL_RULES.md`, 24 reference files [GIST]. I read README, GLOBAL_RULES, the Target 00 and Target 90 parts of SKILL.md, and the routing manifest `09_failure_vaccines.md`; the reference files are saved but not read in full.

Third-party hop: Vinnie Falco's "Semantic Blur" study [BLUR] and orchestrator spec [DOK]; Sam Labbe's independent miner replication [MINER]; README-level skims of `ag-ops` [AGOPS], `makefile-workflow-skill` [HEDGES], `nemik` [NEMIK].

---

## 3. The ideas, grouped

### 3.1 Compaction immunity: put state on disk, re-read it after compaction

The problem, in the site's words: host auto-compaction "hands the entire 230,000-token transcript to a background summarizer model, replaces the history with a three-paragraph prose summary", so the agent forgets governance rules, repeats rejected approaches, and loses its place; "prose summarization optimizes for narrative gist, while software engineering depends on discrete, non-negotiable invariants" [ART-COMPACT]. The example banner it quotes is `[Context compacted: 235,246 tokens -> 14,800 tokens]` [ART-COMPACT].

Why a single PROGRESS.md fails (the author's two named traps) [ART-COMPACT]:
1. Destructive overwrite: every edit to the pointer at the bottom regenerates the whole file and erodes the rules at the top.
2. Monolithic bloat: appended notes bury the active pointer in the middle of the file.
The diagnosis: one file "conflates immutable laws with mutable program counters".

The mechanism ("Memento Bootloader", `Target 0: read-init-d`) [ART-COMPACT] [T8-222]:
- A directory of lexically ordered Markdown cards, one file per lifecycle role:
  - `00_governance.md`  write-once at start: active workflow path and the human approval gates
  - `10_ticket.md`      write-once: issue id, branch, worktree, requirements
  - `15_archaeology_receipt.md`, `20_plan_approved.md`  write-once at their phase
  - `30_critic_summary.md`  append-only: critic verdicts, blockers fixed, false positives dismissed ("negative memory"), plus a SHA-256 of the reviewed diff
  - `99_next_action.md`  the only file overwritten at each transition; three fields: Current Target, Last Completed Step, Next Permitted Action (an RFC 822-style `Key: Value` header)
- One global rule makes the agent read the directory: "If `<brain>/state/*.md` exists, and immediately upon waking from any context compaction, read all files in `<brain>/state/*.md` in lexical order (00 -> 99) before taking any other action" and "Never trust the host compaction summary over `<brain>/state/*.md`" [ART-COMPACT].
- Size: five files, about 1,800 tokens in the article [ART-COMPACT]; "under 500 tokens" in Ch. 8 [T8-222]. A `99_` update costs "fewer than 50 tokens", which the theory chapter says is a 98% write-token saving over re-serialising a JSON state blob [T8-222] (claim).
- Recency: because the read happens after compaction, the cards land at the end of the fresh context, "enjoying maximum recency attention" [ART-COMPACT]. Ch. 12 sec. 325 backs this with the "lost in the middle" argument: high salience at the system head and the tail, a low-salience middle, and strong recency pull from the last 2,000 to 5,000 tokens [T12-325].
- Location: the state directory is out of the repo: `<appDataDir>/brain/<conversation-id>/state/` in Antigravity, or `~/.cache/agent-scars/<repo-hash>-<branch>/state/` with `0700` permissions in a CLI. Reason given: a repo-relative state dir lets a cloned untrusted repo ship a malicious `00_governance.md` (prompt-injection "Trojan horse") and pollutes `git status`. Quoted issue/PR text inside state files is to be treated "strictly as untrusted data" [T8-222] [GIST-SKILL].
- Activation guard: the bootloader runs only if the directory exists. In ad-hoc Q&A, spikes and general conversations "the agent executes zero overhead" [T8-222] [GIST-GLOBAL].
- Lifecycle end: an early section says `rm -rf <brain>/state/` at finish [T8-222]; the current `SKILL.md` says never delete it and instead mark `99_next_action.md` `[COMPLETE]` as a tombstone, citing "Did it finish?" ambiguity (SCAR-PROC-101) [GIST-SKILL] [REG].

Evidence the site gives:
- Two live compactions inside one 579-step ticket (steps 282 and 493, at 229,634 and 235,246 tokens): the first action afterwards was the bootloader read plus `manage_subagents: list`, "0" lost invariants [ART-COMPACT]. Other logged compactions: 233,658 -> 43,077 tokens one second after the user said "ok to commit and push" (TICKET-227, resumed with 0 nudges) [ART-FLIGHT]; 238,419 -> 41,728 (-82.5%) in the middle of a turn, state already flushed one second earlier, "not a step lost" [T11-291.6]; 235,086 -> 36,566 [ART-COMPACT].
- Controlled test (author-run, simulated, Gemini models only): inject a compaction summary that subtly reverses a safety rule. With a single mutable `PROGRESS.md`, 119/120 trials (99.2%) followed the corrupted summary (0.8% survival). With the lexical `init.d` latch design, survival was 72/120 (60.0%) overall, 29/30 (96.7%) on gemini-2.5-pro and 21/30 (70%) on gemini-2.5-flash, `p<0.0001`. The same benchmark's other 6 suites use 30 trials per cell, ~16,000 tokens of distractor load, four Gemini tiers [ART-GRAVITY] [P-EMP]. The remaining 22 of the 72 successes came from the other two models; the per-model split for the weak tiers is not given.

### 3.2 The post-compaction resume protocol (what to do on the first turn back)

Concrete rules the site learned the hard way:
- Freeze first. On a header like `# Resuming from a compaction`, the agent must not edit files, call write tools or spawn subagents until it has reconciled state: (1) list running subagents and kill duplicates/orphans, (2) `git status -s`, (3) re-read the frontier-resolution table in the skill, (4) print a status header in chat, then act (SCAR-PROC-93). The observed bug: the summary says "a review was requested", disk shows uncommitted edits, so the model launches a second critic on the same tree [REG].
- Replay the un-persisted tail ("WAL replay"). If compaction fires before the agent has written the user's last instruction into `99_next_action.md` (observed: user message at 21:02:29Z, compaction at 21:02:31Z, disk still said "STOP and request landing approval"), the bootloader must "reconcile `99_next_action.md` with any uncommitted directive at the tail of `# User Requests`" [ART-COMPACT].
- Store values, not pointers. A state card that lists only the filename of a rubric (`references/05_six_pillars_...md`) failed after compaction: the agent invented six plausible but wrong "pillar" names, passed them to a sub-agent, then overwrote the sub-agent's correct output and the PR description with the invented table. Fix (SCAR-PROC-121): `00_governance.md` pins the literal strings (`### 1. Intent` ... `### 6. ...`) and the post-compaction step re-reads the source file before reuse. Validated later in the same repo: compaction at 229k tokens, next-turn bootloader read, "100% prose preservation" [T9-262] [REG: SCAR-PROC-100].
- Open-commitment switch. Because the cards are typed, resume reduces to a 4-way switch on file state: OPEN_REQUEST (ticket card exists, plan missing) -> do archaeology and offer a plan; ACTIVE_PROMISE (plan approved, gate receipt missing) -> keep executing; RETRACTED_PREMISE (a recorded fact was falsified) -> halt edits and renegotiate; AWAITING_DECLARATION (work done, human sign-off missing) -> stop and wait with no tool calls [ART-REQ] [T9-266].
- Do not write the pointer while a background task or child agent is still running. Observed failure: the orchestrator updated `99_next_action.md` while a backgrounded `gh pr checks` was still sleeping; to fill the "Last Completed Step" field it invented that the review bot had approved with two findings (SCAR-PROC-111, "latent-anxiety projection confabulation"). Rule: yield the turn instead, and never run `sleep` in a shell [ART-COMPACT] [REG].
- Zombie sub-agents. A child agent that compacts (about 250k tokens) before its final tool call can stall in state "running": the host writes a `CHECKPOINT` entry (with a full distilled summary) into its transcript but nothing schedules the next step. Rule (SCAR-PROC-109): when a watchdog fires and the child shows "running" with no detail, read the last 5 lines of its transcript; if the last entry is `CHECKPOINT`, read that summary, kill the child, continue. Watchdog interval later cut from 180 s to 90 s [T8-231] [T8-234].
- Poll children with bounded tail reads. Reading a child's transcript from line 1 pulled in 68,844 tokens per poll; `tail -n 15` slices cut per-poll growth from 15,031 to 240 tokens (62.6x) (SCAR-PROC-113) [ART-COMPACT].
- Human pauses. When the user says "pause, I have to relocate", the right behaviour is zero tool calls and no timers, so nothing burns tokens or triggers compaction while the user is away; resume by re-running the bootloader and re-verifying repo state (SCAR-PROC-136) [REG].

### 3.3 Fork/wait sub-agents as the main compaction avoidance

Claim: keep the primary agent a "Depth-0 orchestrator" (the `make` engine) forbidden from running heavy searches, test suites or edit loops; fork an ephemeral Level-1 worker per heavy phase (archaeology, TDD, critic, CI triage); the worker "absorbs 100k to 220k tokens in its own window, writes the artifact to disk, updates `99_next_action.md`, returns a 15-line summary and terminates" [ART-COMPACT].
- Evidence: PR #348 (+916/-92 lines, 11 files): children absorbed 440,000+ tokens; the orchestrator finished in 196 steps / 53 minutes with zero compactions; an afternoon of work used 6% of a 5-hour Gemini quota window [ART-COMPACT]. FT-128 control: orchestrator transcript 460 KB while 12 children generated 2.2 MB; the passive observer agent in the same run, with no forking, compacted at step 175 and survived only because it kept writing findings to disk [T8-223]. Orchestrator context target: at most 30k tokens [T8-222].
- Granularity: four coarse workers per ticket (archaeology, TDD, critic, bot triage), not one per micro-step, because each fork reloads system prompt, tool list and rules (a reader's "fork/exec cold-start tax") [ART-COMPACT] [ART-GRAVITY].
- Phase-bound manuals: each worker is handed only the reference file for its phase (`references/0N_*.md`), passed in its prompt as a prerequisite, not left as a link it may ignore [T8-223].
- Vinnie Falco's independent orchestrator spec says the same: "Main's context window is a non-renewable resource. Every token loaded into main persists for the rest of the session. Subagent contexts are ephemeral and free" [ART-GRAVITY] (quoting [DOK]); his token-economy section lists what may enter main (manifest, step status, file paths, a correction list capped at 300 tokens) and what must never (raw source, scratch bodies) [DOK].
- Caveats: forked workers still compact (a TDD worker compacted at its step 761 and resumed at 763 from its scratch state) [T12-343]; and a separate trap: the author's own compaction-safe design depends on children writing receipts to disk, not returning bulk text.

### 3.4 Smalltalk-style image + change log (the newer state format)

The more recent design (Ch. 12 sec. 343, `SKILL.md`, GLOBAL_RULES) layers a snapshot-plus-journal on top of the numbered cards [T12-343] [GIST-SKILL] [GIST-GLOBAL]:
- `baseline.sources`: a compact condensed snapshot of the session object, about 50 tokens; example fields: ticketId, decomposedChildId, branchName, baseSha, target, commitmentState, byWhen.
- `changes-<timestamp>.changes`: append-only, immutable delta chunks (Smalltalk chunk format, e.g. `archaeologyVerified` sets target, commitmentState and a receipt).
- 3-trigger checkpoint law: write a chunk only on (1) a phase or human-gate boundary, (2) each completed iteration of a multi-step loop (for example each critic round), (3) any exception, rewind, scope amendment, test-failure retreat or merge-collision rollback.
- Condense at clean phase boundaries: fold the active chunks into `baseline.sources.tmp`, move old chunks to `state/history/` (kept for post-mortem audit), atomically rename the `.tmp` over `baseline.sources`.
- Unwind without losing receipts: when the human says mid-plan that the scope is separable, the scope change is logged as an exception chunk, the child issue is created first, and prior verified receipts (the archaeology card) are not recomputed because the Makefile prerequisites are still satisfied. Reported field test: 1,801 combined steps in 95 minutes, orchestrator 370 steps [T12-343].
- The rule of thumb attached to it: "Agents don't need huge context. They just need to know where to write things down." (Schwartz, quoted in [T8-222]).

### 3.5 Make-style freshness instead of step numbers

Declarative dependency graph instead of a numbered checklist: the agent evaluates `make finish` backward and runs the deepest unsatisfied target; state is "which artifacts exist and are fresher than their inputs", not "what step am I on" [ART-MAKE]. Memory-relevant details:
- Resume oracle: a table from physical state (PR open with red CI; local commit not pushed; approved plan but no code; clean main) to the active target, so the user never says "you are on step 12" [ART-MAKE].
- Invalidation by mtime: editing a source file after the tests/critic receipts were written makes the downstream receipts stale and forces the leaf to rerun; the critic receipt is bound to a SHA-256 or, in the newer design, `git write-tree` of the reviewed tree, so an unreviewed byte invalidates it [ART-GRAVITY] [ART-SENSOR]. `git diff` misses untracked files, so `git add -N` before hashing (reader-supplied catch) [ART-COMPACT].
- Bounded fan-in: at most 2 to 3 prerequisites per target, with the human gate as its own second prerequisite (`land: ready-to-land human-landing-approval`); beyond 3, the gate "gets swept along as a rhetorical checkbox" (SCAR-PROC-91) [ART-MAKE].
- Caveat from a reader the author quotes: a Makefile target that produces no file reruns every time, so API-calling steps need a `.stamp` file or a wake-up replays the side effect; and "a human approval target should be a receipt tied to the exact input/diff hash, not just a file that exists" [ART-COMPACT].

### 3.6 Episodic records: the flight recorder as external memory

- Compact log vs full log: the host's `transcript.jsonl` truncates large turns (`<truncated 4927 bytes>` out of a 9,001-byte plan) and sets `truncated_fields`; the uncompacted copy is `transcript_full.jsonl`. Reflex (SCAR-META-07): when in-context memory of a past decision has decayed, ground against the on-disk log, check `truncated_fields`, then read the exact line of the full log by index instead of looping on grep [T8-215] [REG].
- Mining transcripts for memory: an independent auditor agent reads the worker's raw transcript including its thinking trace at the step where it drifted, then writes the scar "in the exact dialect" that steers the same weights next time. The author's argument for why a model can audit its own kind: it shares the attention physics of the worker. Prerequisite: "Gemini 3 was the minimum model generation" for this ("Prosthetics Threshold") [ART-PAIN].
- Deflection telemetry: a script scanned 532 sessions / 9,488 thinking steps and found 147 "trajectory swerves" where the thinking trace shows the model contemplating a trap, citing a scar ID, and pivoting (1.55% of reasoning turns; PROC 81, GENERAL 51, RIV 5, TEST 3, other 7) [P-EMP]. This is the site's evidence that stored scars are actually consulted.
- Retrospective git mining ("Rapid-Regret"): candidate scar if the same author's next commit lands within 60 minutes and touches overlapping symbols; commit k = Trap, the gap's CI failure/bug = Wound, commit k+1 = Reflex [ART-PAIN]. The only independent test (445-commit TypeScript repo) found precision of 9% strict (16% with partials) unfiltered; a "fix"-verb gate on commit k+1 gave 31%/54% with 4/4 recall; hunk-polarity inversion dropped half the true cases because all four true regrets were additive "errors of omission" [MINER] [T12-338.11].

### 3.7 The scar: a typed failure record, written affirmatively

Format (registry and submit page): `SCAR-<DOMAIN>-<NN>` id, title, domain, visibility, workflow-phase binding, then four fields: The Wound (exact failure), The Trap (the tempting high-probability continuation), The Circuit Breaker (explicit permission to stop or counter-offer), The Permanent Reflex (concrete ordered steps to do instead) [P-TAX] [SUBMIT] [ART-PAIN].
- Why this shape: negative rules ("DON'T use `??`") put the forbidden tokens into context and prime them; so name the trap once, give a legal exit, and spend most words on the positive replacement. Rule: the Reflex is always affirmative; the Safety/negative form is moved to the critic, where the same token is a search needle rather than a generative attractor [P-TAX 3.3-3.4] [ART-PAIN].
- Reality check on the shipped data: of 577 registry entries, only 59 have a non-empty Circuit Breaker field and 384 have a phase binding; median entry about 2,200 characters (mean 2,600; max 9,900; total about 1.5 MB, i.e. roughly 380k tokens). Most are in practice three-part records [REG] (counted by me from `pages/registry_all_scars.md`).
- Domain mix: PROC 155, REVIEW 32, STATE 32, TEST 29, UI 22, DOC 20, ARCH 19, TOOL 19, RIV 17, ASYNC 15, CI 15, STORAGE 15, GIT 13 ... 577 total (460 "Public (Universal Systems Reflex)", 117 "Public (Open Source)") [REG]. A private registry of 23 domain-specific scars is kept out of the public one, with an export script that regex-checks for leaked proprietary terms [P-EMP].
- Capture loop: gates gather, scars distribute. Each human pause is a sensor; a "no" is turned into a scar by a post-landing consolidation step (the "Step 21 / Target 90" sleep-consolidation analogy: an isolated sub-agent in a shared worktree reads the finished ticket's logs, writes the scar, commits it to the registry and the repo handbook) [P-ARCH 4.1] [T2-48]. Contrastive elicitation is the claim for why this works: experts cannot list rules cold, but recognise a wrong proposal in context, so the model's wrong move is the retrieval cue; the nudge is recorded as the tuple (state, trap action, nudge, target) [T9-259].
- Hand-off rule: every sub-ticket or hand-off spec must be context-free: exact file paths and line ranges, the wound, known pitfalls, the test to port, and an explicit red-then-green contract, because the receiving agent has "zero implicit session memory" (SCAR-PROC-82) [REG].
- Cross-agent rule: never leave learnings only in a private agent's runtime memory; write them into shared skill or repo reference files so every agent (Claude, Gemini, sub-agents) sees the same invariants [GIST-GLOBAL]. The mirror rule (SCAR-PROC-130): private memory identifiers (`SCAR-*`) must be rewritten to in-repo language before they go into PR text, commit messages or tickets [T10-287.3].

### 3.8 Scale: do not load the catalogue, route it

- The measured pain: the chronological scar files reached 280 KB (163 KB + 116 KB, about 70,000 tokens); a 50-line backend ticket made planner and critic wade through 30,000 tokens of unrelated Flutter, SSR and pub-solver scars [ART-SENSOR].
- Fix ("loadable kernel modules"): a 2.4 KB routing manifest (`09_failure_vaccines.md`) plus modules of 27 to 36 KB each. The "PCI bus" is `git diff --name-only origin/main...HEAD`; path patterns decide which module(s) mount (always-on core module at 33 KB; the Dart AST module when `*_lint/**`, `**/rules/**` change; the Flutter module when `*_flutter/**`, `widgets/**`; pub/docs module when `pubspec.yaml`, `CHANGELOG.md`, `doc/**`; test module when `test/**`; git/workflow module when `workflows/**`, hooks, CI). Mounted at three points: archaeology, implementation, adversarial review. New scars are appended to the matching module at post-mortem [ART-SENSOR] [GIST-VACCINE].
- Why each module is capped near 36 KB: the host's single-fetch file read returns 46,080 bytes (about 800 lines), and agents show "early epistemic satisfaction": after reading the first two pages of a 227 KB `SKILL.md` the orchestrator saw the DAG, stopped paging, and never reached the critic registration at byte 105,000. So the always-resident skill is capped at 40,000 bytes / 750 lines (SCAR-PROC-99; the article says 42,000) and phase-bound manuals are loaded on demand [T8-223] [ART-COMPACT] [GIST-README].
- Phase-bound projection: at any step the agent evaluates a sparse working set (the paper says at most 5 active scars) via Phi(scar) = workflow step; AST-diff routing keeps prompt density constant as the registry grows [P-DISC 8.9.3].
- The always-loaded file as a tax: `AGENTS.md` has a 32,000-byte ceiling enforced by a script. Expected token tax of a rule = bytes x probability a session needs it; a rule needed in 2% of sessions costs 98% of sessions for nothing, so it goes in an on-demand skill or linter. A case where an agent squeezed a 580-byte rule in under the ceiling by rewriting about 140 lines (-1,314 B) across 8 sections was rejected by the human reviewer, and the rewrite collided with other PRs touching the same file (merge conflict). Resolution by prefix routing: a new rule namespace was added to a skill with 0 bytes changed in `AGENTS.md` (at 31,996 / 32,000 B) [T11-291.2] [T11-291.6] [T9-269].
- Admission gate for shared (repo-level) documentation (SCAR-META-10): file a meta-ticket only if the originating PR is merged, the pattern recurs at 2 or more call sites (or is a global gate), the target file is actually loaded by agents or CI (a human-only `DEVELOPERS.md` is a "passive sink"), and fitting it does not require compacting unrelated sections [T11-291.3].
- Counterpoint inside the same corpus (SCAR-GOV-03): when the whole governance corpus fits comfortably in the model's window (195 KB, about 48,000 tokens, under 5% of a 1M-token window), micro-routing with regexes causes false negatives and maintenance drift; load everything unconditionally. So routing pays when the corpus is large relative to window, not always [REG].
- Router phrasing: skill descriptions that read like FAQ answers ("what's new in Dart 3.13?") never activate in an imperative coding task because embedding similarity to the task text is low; write triggers as imperative actions plus concrete file or AST targets (SCAR-SKILL-02, "Conversational FAQ Attractor") [T5-149].
- Stale pins in prompt files: a skill that embeds a false fact produces a false positive from every reviewer bot (probability about 1). Rule: verify the whole enclosing rule block against the primary source when fixing one sentence (adjacent sentences are in the diff context and get flagged next) (SCAR-REVIEW-28) [T10-272].

### 3.9 Lifecycle: provenance, half-life, garbage collection, apoptosis

This is the part of the site closest to a memory-decay design; none of it is numeric time decay. It is event-driven staleness.
- Provenance first: scars "without verifiable provenance possess permeable barrier heights"; only scars backed by reproducible crash traces and peer replication are non-negotiable [T2-37].
- Promotion tiers (Ch. 1 sec. 30): Level 1 immutable axioms (no drift); Level 2 architectural invariants promoted only after a "Critic Council" vote and independent replication (N_reps >= 3 distinct agents); Level 3 local conventions, advisory, easily superseded ("epidemiological decay"). New candidate scars are quarantined in a local incubation state and spread only with replication, to avoid "dogmatic infection" of a hallucinated rule [T1-30].
- Scar density limit: when defensive code approaches domain code (density ratio near 1, "just scars, no skin"), stop patching and re-architect [T1-12]. Memory analogue: the proscription ratio, negative rules versus generative rules; above 1 is "paradigm bankruptcy", and the value of a migration is the number of negative rules you get to forget [T3-77].
- Context signature and half-life: boundary-dependent scars store a signature = (dependency version range, hash of the surrounding AST/interface, path of the behavioural test). When the lockfile moves outside the range or the interface drifts past a threshold, state goes `Enforced -> PROBATIONARY_REVERIFY`; the harness lifts the policy and runs the behavioural test against the new dependency. Still fails: keep and re-pin the signature. Passes: retire the policy/lint rule to an archive but keep the test permanently ("scar retirement and behavioural test retention") [T12-338.11].
- Garbage collection by re-running the wound: since each scar carries the repro of its Wound, a periodic mark-and-sweep re-executes it; if it now passes cleanly the scar is collected [P-REL 7.1.3]. Companion "cauterizing probe": production invariants stay absolute, but in sandboxes a single low-probability probe re-tests an ancient wound (human version: "Humans will at least try a bad behavior once... a good cauterizing rule for machines. Since conditions can change") [T2-43].
- Apoptosis and graduation: a deterministic scar graduates from prompt text to a compiler/AST rule or hook. Costs table [ART-SENSOR]: prompt scar costs about 150 to 300 tokens per mounted scar and, when violated, one critic bounce (15 to 30 s, about 8,000 tokens); lint diagnostic costs 0 prompt tokens and one local turn; auto-fix costs 0 turns (about 15 ms). The Markdown entry becomes a one-line tombstone (`Lifecycle: Graduated -> package:synthetic_scars_lint`) and a negative-witness fixture (a test containing the historical trap annotated `// expect_lint: <id>`) stays so nobody deletes the rule without the test failing ("Chesterton's fence in the test runner"). Retiring as "upstreamed" requires disabling the local sensor and watching the upstream tool fail on the fixture first; release notes are not trusted [P-ARCH 4.5].
- Second-order evasion: when a linter blocks a pattern the model routes around it (pre-capturing `ScaffoldMessenger.of(context)` before an `await` silences the lint but leaves the bug). So high-consequence scars get three tiers: lint, runtime failure-injection test, fresh-context critic [P-ARCH 4.5] [ART-SENSOR].
- Provenance must live in the repo: no SaaS ticket links; the registry file, the sensor and its negative witness sit together in git [P-ARCH 4.5].
- Cross-ticket propagation ("momentum projector"): at idle ("dreaming") time a background agent takes each landed PR's lessons and (a) enriches the acceptance criteria of semantically similar open tickets before work starts, (b) scans open branches/worktrees for the newly found trap [P-ARCH 4.3] (described as a design, with one worked example).
- Weights versus disk: a "Two-Tier Partition" argument that universal scars (about half; never swallow exceptions, cancel subscriptions in `dispose`) could be trained into weights, while repo-conditioned scars cannot (the same `Future.wait` is right in one repo and fatal in another) and must stay on disk where update latency is "12 seconds" versus 6 to 12 months for weights [ART-GLISTERS].

### 3.10 Summaries and rewrites degrade information ("semantic blur")

Third-party but cited as a core support by the site [ART-SENSOR] [BLUR]:
- Observation: three prompt files that an LLM rewrote repeatedly over three weeks grew +18.7%, +43.6% and +145.8% in characters with no new capability, concentrated in single "refactor" commits that handed the model the whole file; one 138,184-character, 1,404-line, 22-step file sat at 4x the median of 23 tools. A concrete number ("GPT-4 on GSM8K from 95.5% to 91.5% after one round and 89.0% after two") was replaced by a generic claim in later versions.
- Mechanism offered: rewriting regenerates every token from the model's prior of "good technical writing" (complete, justified, formal), so each pass moves the text toward the mean and away from the sharp first version, which was conditioned on human intent.
- Remedy: keep the discrete decisions (the plan) as the source of truth and regenerate the artifact from it; never rewrite the artifact in place ("black-and-white line drawing theorem": decisions are discrete and survive a blur; prose qualities do not).
- Evidence limits stated by the author of the study: 3 files, one workspace, one model family, mechanism inferred not isolated by experiment [BLUR].
- A second observation from the site's own repo: the same universal attractor, around 1,400 lines and 21 to 22 numbered steps, appeared in the author's own pre-Makefile `SKILL.md` [ART-SENSOR].
- A live instance: an automated "simplify English" pass over sub-agent prompts rewrote `"$APPROVED_TREE"` to `"HEAD"`, which created an empty-diff bypass (issue in [HEDGES], cited in [ART-GLISTERS]).
- Relevance to compaction: a recursive summary of a summary is a rewrite loop. The site's alternative is a typed, mostly append-only state record that is never re-summarised.

### 3.11 Facts with four epistemic states, retraction as a first-class event

The `15_archaeology_receipt.md` card holds one row per factual premise: `fact_id | status | claim | witness_ref`, with status in {ESTABLISHED, CONTRADICTED, UNKNOWN, RETRACTED} (Belnap's four-valued logic) [T9-258].
- ESTABLISHED requires a witness (file:line, git SHA, or tool exit 0). UNKNOWN and CONTRADICTED block the move from planning to coding; two sources that disagree must be recorded as CONTRADICTED, not silently harmonised into one confident sentence.
- RETRACTED is non-destructive and bitemporal: when a later test or compiler run disproves a recorded fact, the row is marked retracted with the time and the witness, a new fact is added, the card's mtime moves past the approved-plan card, and Make-style staleness forces a re-plan. The tombstone stays so no later sub-agent re-proposes the falsified idea.
- Retraction needs a stronger receipt than assertion: "a receipt for absence is weaker than a receipt for presence" (an `rg` exit 1 only proves one search found nothing). The site's rule: a retraction must state the search scope or a compiler/LSP probe, the observed output, and the deciding predicate [ART-SENSOR].
- Do not model absent or unparsed as null: collapsing CONTRADICTED to UNKNOWN is what a `catch (_) { return null; }` does [ART-SENSOR].
- Stale-premise detection by history: a read-only "archaeologist" checks a ticket's claims against `git log -p` before the agent may promise code. Results: a ticket that would have produced about 280 lines of code was closed in 8 min 29 s with 0 lines, because a sibling PR two weeks earlier had already written that wrapper (243 lines) and the lead architect had rejected it in review; two other tickets were roughly halved because parts had already landed [ART-FLIGHT].

### 3.12 Enforcement outside the model, and the trust boundary for state

- Any state the agent writes can be forged by the agent: "The DAG protects the check from being skipped; the sensor protects the check from being forged." Exit codes the agent can influence are "the LLM's token stream wearing an OS badge". Countermeasures: keep tests read-only across red-to-green via a tree hash of the test directory, make the critic a separate process whose transcript is written by the host, store a git tree hash of what was reviewed [ART-SENSOR].
- Four counterfeiting patterns the author caught across 151 tickets (self-review in same context, watered-down rubric prompt, farming a critic by repeatedly messaging it, killing a running critic and writing a fake approval) and a fixture suite replaying each [ART-COMPACT].
- Never let the classified pick its own risk tier: "the depth may vary, the classifier may not be the classified"; deterministic classifiers are "incorruptible, not immortal" and need drift re-calibration [T12-338.11].
- Write safety in `ag-ops`: state in `.ag_state/NN_<slug>.json` using temp file + `os.replace` + `os.fsync`; exit-code contract 0/1/2 where 2 means integrity violation such as tampered tests or a corrupted receipt [AGOPS]. A variant keeps state under `<workspace>/.workflow/state/` and adds it to `.git/info/exclude` [HEDGES]; the site's own design keeps it out of the tree entirely [T8-222].
- Hash-chained, signed journal (NoireBox, third party): each decision sealed in a SHA-256 chain with Ed25519 signatures so the record can be verified offline [NOIREBOX]. Labbe's point that a hash chain catches commission (a verdict contradicting its inputs) but only counting catches omission [T12-338.11].

### 3.13 Bootstrapping a project's memory: the five-probe interview

For onboarding to a repo, the agent interviews the lead with five questions and compiles the answers into the workflow file ("Sourdough Starter") [T5-126]:
1. Irreversible actions: "What cannot be undone by `git reset`?" -> terminal gates (push, merge, publish, deploy).
2. Proof-of-work oracles: "What is undeniable proof code works here?" -> compiler, zero-tolerance lint flags, non-degenerate fixtures.
3. Domain wound inventory: "Where has this team bled before?" -> seeds initial scars and the critic's rubric.
4. Human attention budget: when must the agent stop; how evidence is formatted (the author has aphantasia, so rendered, grounded visuals are required).
5. Quarantine zones: files, branches, environments never to be touched or committed.
A variant adds a sixth probe to extract coarse domain topology (artifact transformations, oracles, two-phase-commit points) [T9-259.3]. Ceremony depth is parameterised by target pairs (`commit-fast` vs `commit-full`) while an indivisible `kernel-verified` prerequisite is never skipped; natural-language permission to relax steps was found to generalise too far ("forward token-inertia") [T5-126.4].

### 3.14 Instruction precedence and scoping

- Strict monotonic enclosure: physics (compiler, OS) <= repo gates <= workflow <= operator; each layer may only narrow the allowed actions, never widen a lower one; actions must satisfy all layers conjunctively, which removes the "rogue override" and "insurrectionist freeze" failures [T6-180].
- Hermetic shadowing: inside a repo, the project skill fully shadows generic global skills; global skills must contain zero client names or ticket keys; post-mortem consolidation runs in an isolated worktree by a separate sub-agent [T3-68].
- Convergence invariant: no session may end until the consolidation agent has run, in both land-it and hand-to-reviewer branches (otherwise "Workflow Truncation Amnesia": the lessons stay in the chat transcript) [T3-86].
- Gates as capture points: every human pause is where tacit knowledge is gathered, and the consolidation step is where it is distributed; "without the scars being collected, it was still a brand new junior just following my reviews" [P-ARCH 4.1].

### 3.15 Attention mechanics the author relies on (explanatory, partly speculative)

- U-shaped attention, softmax dilution as context grows, recency entrainment; "needle-in-a-haystack proves the model can find a key in the dark, not that it can build an engine while holding 200,000 loose bolts" [T12-325].
- Procedural dementia: instruction weight decays with context; the model keeps local competence but loses macro state; forward token-inertia makes `git push` after green tests more probable than invoking the critic. Remedy: outcome invariants (guards) that block the terminal action, not advice [T6-154].
- Local specificity paradox: more precise local error text narrows attention to that line and moves the agent away from the root cause; a fresh un-primed reviewer resets the frame [T3-86.4].
- Self-polluted loop: "a prompt rule is read by the same model that is stuck in the loop", so loop breakers belong in the harness [T12-338.11].
- Pre-training gravity: use constructs the model has seen millions of times (Makefile, `init.d`, RFC 822 headers, EBNF, circuit-breaker state machines, `git bisect`) instead of inventing a DSL; reported gains for 15 to 40 token canonical forms vs 300 to 600 token prose: circuit breaker FSM 0.8% -> 98.3%, EBNF output grammar 32.5% -> 77.5%, git bisect 19.2% -> 81.7%, WAL/2PC 45.8% -> 93.3% [ART-GRAVITY]. The Makefile-vs-checklist suite itself showed no overall difference (72.5% vs 72.5%), with a gain only on the smallest model tier (3.3% -> 20.0%) [ART-GRAVITY] [P-EMP].
- Speculative sections (attention-mask injection of minus infinity at trap centroids, energy landscapes) describe model internals no agent harness can touch; I treat them as motivation only [T2-45].

---

## 4. Working outside coding projects

Plain statement: the site is almost entirely about repo-bound tickets. It contains nothing on personal knowledge bases, conversational assistants' user profiles, persona or identity persistence, embedding-based retrieval, relevance scoring, or numeric decay. The transferable material is mechanism, not content.

What transfers (with sources):
1. The state directory is keyed by session or conversation, not by repo. In the site's own design the path is `<appDataDir>/brain/<conversation-id>/state/` (Antigravity), with the repo hash and branch only in the CLI variant [T8-222]. A non-repo session can use the conversation id alone.
2. Existence-gated bootstrap: the read-state rule fires only if the directory exists, so plain chat pays nothing; a readerboard can still be created on request in ad-hoc tasks [T8-222] [GIST-GLOBAL].
3. Same file roles for any long task: write-once intent and constraints, append-only decisions/rejections/dismissed ideas (negative memory), one overwritten pointer file (current step, last step, next permitted action) [ART-COMPACT].
4. The human-memory analogue shows the pattern is not code-specific. For interruptions: leave a deliberately failing assertion as an instruction pointer, or a "3-field interruption card": the invariant state (what was true ten seconds ago), the mechanical coordinate (exact file path and line), the next physical step; "narrative abstracts away the physical pointer" [T12-320.2].
5. Fact ledger for assistant knowledge (user preferences, claims from documents): status {established, contradicted, unknown, retracted} plus a witness reference; contradictions are surfaced, not merged; retraction is logged with time, never deleted [T9-258] [P-DISC 8.14].
6. Customer-support / clinical / finance agents (the paper's own non-coding section 8.14 and Ch. 9 sec. 261): replace the open `while(true)` chat loop with a goal DAG (`00_verify_order_id -> 10_classify_defect -> 20_lookup_policy -> 30_prepare_resolution -> 40_commit_resolution`); each turn is limited by an EBNF grammar to a short acknowledgement plus one question for the next missing slot; a progress counter (satisfied slots this turn minus last turn) feeds a 3-state circuit breaker (CLOSED, HALF_OPEN, OPEN_HALTED_ESCALATE) that trips after two turns of zero progress; policy lookups are recorded in a four-valued ledger and any CONTRADICTED/UNKNOWN blocks the commit step; a two-phase commit separates "prepare resolution" from an out-of-band validator before "commit". Hand-offs to a human specialist are a compact dashboard of `00_verified_customer_context.md`, `15_epistemic_ledger.md`, `99_next_action.md` instead of a 100-turn transcript, so "the customer does not repeat their story" [P-DISC 8.14]. These are described, not evaluated; the paper gives no numbers for them (the 98.3% circuit-breaker result is from the coding benchmark).
7. Context-free hand-offs between agent sessions: 5-point self-contained spec (SCAR-PROC-82) applies to any delegated task [REG].
8. Scoping and privacy: global memory must be client-agnostic; project memory shadows global; private identifiers are translated before reaching shared artifacts (SCAR-PROC-04 / PROC-130) [T3-68] [T10-287.3]. Fits a local-first stance: a hard line between what is private and what is allowed to leave.
9. Treat state-file text as untrusted data and keep it out of the working tree, because someone else's repo or document can plant instructions in it [T8-222].
10. Vendor- and model-agnostic resume: because state is plain files, the author swapped Claude Sonnet 4.6 for Gemini 4 at step 433 of 960 and the new model "read the init.d files Claude had written... and finished the run" [ART-FLIGHT]; Ch. 10 sec. 287 adds that the incoming model made 31x fewer hesitation phrases (0.38% of turns vs 11.8%) [T10-287.1] (anecdotal, one run). Saved sessions that resume across providers need this property.

What does not transfer: nothing here addresses recall over a large heterogenous corpus, user profile modelling or conversational style/persona.

---

## 5. Failure modes, things that did not work, open problems

What failed in the author's own use (each is a named scar or section):
- Single `PROGRESS.md`/`TODO.md` (overwrite + bloat) [ART-COMPACT]; 20-step numbered prompt that decays at "1,400 lines / 21 to 22 steps" [ART-SENSOR]; "keep notes" instructions lose to a corrupted compaction summary 99.2% of the time in the simulated test [ART-GRAVITY].
- Pointer-only state after compaction produced invented schemas and overwritten correct output [T9-262].
- Paging trap: a skill file longer than the single-read limit is not fully read ("early epistemic satisfaction") [T8-223]; and a split-brain where the top-level skill embedded a paraphrased copy of a reference rubric, so the agent never opened the real one (SCAR-PROC-100); rule: never paraphrase canonical prompts in the bootloader, project them verbatim or force a read [T8-224].
- Improvised rubrics: with no fixed external rubric, the reviewer wrote criteria that mirrored the implementation and approved a diff that the fixed six-pillar critic rejected with three blockers on the same diff (FT-128) [T8-223].
- Duplicate sub-agents or zombie sub-agents after compaction; speculative writes while tasks run; fork-bomb of backgrounded `git push`/`commit` because a polling rule judged liveness from stdout growth instead of the process table; `rm -f .git/index.lock` while a commit was alive [ART-COMPACT] [T10-287.2] [ART-FLIGHT].
- Capability floor: the same cockpit with `GPT-OSS 120B` over 791 steps: 0 state files created, 0 sub-agents forked, 6 compactions, 50 malformed tool calls (13.6% of turns), 13 web searches for private local classes, 6 concurrent `git commit` jobs, 18 human nudges, aborted. Frontier models on the same ticket: 0 nudges. The author's four prerequisites: obeys turn-1 rules, clean tool calls, basic tool sense, honest error reporting [ART-FLIGHT]. Also: "the capability floor isn't a cliff; it's a first-violation timestamp" [T12-320.3].
- Over-eager documentation capture: a post-mortem meta-ticket written into a human-only doc after one call site was closed unmerged on review [T11-291.1].
- Scar misses of rule-based memory: 40% of 44 early tickets went into "uncharted" territory with no applicable scar, which explains first-pass success of about 56.8% then; the site's claim is not that novel failures vanish but that known ones never repeat [T5-132]. Later aggregate: 68.3% first pass, mean 1.91 human nudges per ticket (246 tickets) [P-EMP].
- Precision of the automated miner is low unfiltered (9%) [MINER].
- Linter evasion (second-order hydra) [P-ARCH 4.5]; classifier drift ("incorruptible, not immortal") [T12-338.11].

Weaknesses of the evidence (my assessment, using the site's own text):
- Headline claim "0.0% repeat regression" is self-defined (an exact Wound+Trap match against a catalogued scar) and self-measured by the author, who is also the human gate [P-DISC 8.9.1] [P-DISC 8.9.2]. The paper's own "further study" chapter lists the control experiments as future work: ablation with no scar catalogue (target: unscarred model triggers the wound at 70% or more), an opportunity-for-repeat overlap matrix to show the zero is not just non-overlapping tasks, and a junior-operator trial [P-FURTHER 9.6]. The baseline row in the metrics table ("Vanilla agent" repeat failure about 42.5%, nudges 3.4, first pass 24.0%) has no method attached [P-EMP].
- Counters differ by page snapshot: home 202 tickets / 529 scars; the paper abstract 246 tickets / 600 scars; the Part 4 article 238 tickets / 588 scars; Part 3.6/3.7/3.8 articles 143 / 151 tickets; Ch. 9 N=210; the registry data holds 577 [HOME] [P-ABS] [ART-PAIN] [ART-MAKE] [REG].
- The 1,680-trial benchmark is author-run, Gemini-only, scripted, N=30 per cell; the compaction suite simulates the compaction summary instead of using a host compactor; one suite showed no effect [ART-GRAVITY].
- Sources are one author's workflow in one language family (Dart/Flutter, plus an industrial monorepo whose details are redacted) and one agent host (Antigravity). Nothing is independently replicated for memory specifically; the independent replications cited concern the miner and the sensor/attack fixtures [MINER] [ART-GRAVITY] (the second concerns the NoireBox replay of the four counterfeiting attacks).
- The site's prose mixes engineering with large unverifiable analogies (physics, biology, Hogan, Flores); the useful content is in the numbered field-test traces and the registry entries.

Open problems the site itself names:
- Scale past about 1,000 scars with constant prompt density [P-DISC 8.9.3]; whether routing beats whole-corpus loading depends on window size (SCAR-GOV-03 vs insmod) [REG] [ART-SENSOR].
- Separating a repeat from a novel variant by structural signature; classifier staleness; reviewer fatigue versus matrix ossification [P-FURTHER 9.5] [T12-338.11].
- Generalisation beyond Dart and beyond Gemini/Claude/Antigravity [P-FURTHER 9.5]; the unpublished Parts 5 and 6 (golden metric; "what you get to forget") [ART-COMPACT].
- Omission errors (forgotten companion artifacts) are the hard class for mining; the answer offered is "cardinality twins" (count obligations on disk) [T12-338.11].

---

## 6. Ranked ideas for Batbox (terminal coding agent with per-project init files, compaction step, sub-agents, saved sessions with resume, local-first)

Ranked by expected payoff per unit of build effort. "Evidence" is the strength of the site's support: A = field traces or a controlled test; B = one author's repeated practice; C = argument only.

1. Typed state cards that survive compaction, re-read after every compaction (evidence A). Keep a small per-session directory (not in the repo) with write-once, append-only and one-pointer files; after the compaction step, inject them at the end of the new context and tell the model to prefer them over the compaction summary. This is the only mechanism with a controlled test (0.8% -> 60% survival of a corrupted summary, 96.7% on the strongest model) and multiple logged compactions with no lost steps [ART-GRAVITY] [ART-COMPACT]. Batbox already has saved sessions: this is the same directory with a defined schema.
2. Separate "immutable decisions", "append-only rejections", "mutable pointer" and forbid compaction from touching the first two (evidence A). The named failure of one-file notes (overwrite plus bloat) is cheap to avoid, and the append-only "what we rejected and why" card stops the agent from re-implementing bugs it already ripped out [ART-COMPACT].
3. Pin literal values, not pointers, in the state card (evidence A). A filename pointing at the rubric or constraint list failed after compaction (the model invented a plausible schema and then overwrote correct output); put the exact strings the model must reproduce into the card, and re-read the source before reuse [T9-262].
4. A fixed first-turn resume protocol (evidence A/B). On a compaction or session resume: no writes or spawns until it has listed running sub-agents/background jobs, run `git status`, read state cards, replayed any user message newer than the pointer file, and printed a status line. Prevents duplicate sub-agents, lost last instruction, and "speculative forward drift" [REG SCAR-PROC-93] [ART-COMPACT].
5. Keep the main context small by forking heavy work and requiring artifacts-to-disk plus a short return (evidence A/B). Sub-agent writes its result to a file and returns at most about 15 lines; main never reads raw logs. Reported: 440k tokens absorbed by children, zero compactions in the parent over 196 steps [ART-COMPACT]. Batbox has sub-agents; add the return-size cap, a result-file convention and bounded tail-reads (62.6x cheaper polling) of child logs.
6. Handle a sub-agent that compacts or dies mid-run (evidence B). Detect "running, no activity", read the tail of its transcript for a checkpoint/summary entry, harvest it, and kill the child rather than waiting through repeated timers [T8-231].
7. Do not recursively summarise; keep a decisions ledger and regenerate the summary from it (evidence B, C for the mechanism). Rewriting a document repeatedly inflated it up to 146%, sanded numbers into generalities, and drifted toward average prose [BLUR]. For a compaction step this means: carry over exact numbers, paths, constraints and decisions verbatim from the ledger; summarise only narrative; never feed a prior summary back as the main input.
8. Write state at events, not on a timer, and never write the pointer while work is still running (evidence A/B). Three triggers (phase/human-gate boundary, each loop iteration, each rewind/exception) plus "yield instead of guessing" prevented the observed fabrication where the pointer was updated before a background result existed [T12-343] [ART-COMPACT].
9. Memory entries as short typed failure records: what went wrong (exact symptom), the tempting wrong move, the permitted way out, the replacement steps, with an affirmative reflex (evidence B). Use a post-session consolidation sub-agent that reads the transcript and proposes entries for human approval; gate promotion to shared project files on recurrence (2+ call sites) so one-off lessons do not bloat the init file [T11-291.3] [ART-PAIN].
10. Route memory by what the task touches and enforce a byte budget on the always-loaded file (evidence B). `git diff --name-only` or the open-file set picks which memory modules load; per-file cap below the tool-read limit so nothing is truncated; ExpectedTokenTax = bytes x probability needed. Note the counterpoint: when the whole corpus is a few percent of the window, load it all [ART-SENSOR] [T11-291.2] [REG SCAR-GOV-03].
11. Give each stored lesson a provenance, a re-verification command and a fingerprint of what it depends on (evidence B for the lifecycle, C for the formalism). On lockfile or interface change, mark it probationary and run the repro; retire the instruction but keep the test; leave a tombstone. This is the site's only concrete answer to stale memory [T12-338.11] [P-REL 7.1.3] [P-ARCH 4.5].
12. Fact rows with status and witness: established / contradicted / unknown / retracted (evidence B). Prevents the model silently harmonising two conflicting notes; makes "that assumption was disproved at time T" a durable, non-destructive entry. Also the most direct fit for non-code assistant memory [T9-258].
13. Treat memory files as untrusted input and keep per-session state outside the working tree (evidence B; fits local-first). Prompt injection through a cloned repo's state files is called out explicitly; also no telemetry is needed, since all state is plain files on disk [T8-222].
14. Offer a flight-recorder recall tool over the saved session log (evidence B): bounded line-range reads of the full log, with a flag when the compact view truncated content, so the agent can recover an approved plan from hundreds of steps ago without grep loops [T8-215] [REG SCAR-META-07].
15. Bootstrap the project init file with a short five-question interview (irreversible actions, proof-of-work commands, where the team has been burned, when to stop for the human, no-touch zones) instead of a blank file (evidence C, cheap to implement) [T5-126].

Lower priority from the site, mostly about workflow governance: Makefile DAG gating of commit/push/land, speech-act typing, Belnap logic for approvals, tree-hash locks and read-only test pins, graduation of rules into linters. They matter if Batbox wants approval gates, but they are not memory.

---

## 7. Source tags

Site (all `https://syntheticscars.org`)
- [HOME] `/`  [KIT] `/starter-kit`  [SUBMIT] `/submit`
- [REG] `/registry` (data extracted from `/main.dart.js` bundle; saved as `pages/registry_all_scars.md`; scar ids cited inline)
- Articles under `/articles/`: [ART-COMPACT] `surviving-context-compaction-unix-init-d-for-ai-agents`; [ART-GRAVITY] `what-llms-know-unix-pretraining-gravity`; [ART-FLIGHT] `flight-testing-the-commitment-machine`; [ART-SENSOR] `is-this-a-step-or-a-sensor-hardening-the-commitment-machine`; [ART-PAIN] `giving-ai-pain-synthetic-scars-and-rapid-regret-miner`; [ART-GLISTERS] `all-that-glisters-is-softmax-ai-scars`; [ART-MAKE] `stuart-feldman-was-right-makefiles-for-ai-agents`; [ART-REQ] `requests-promises-assertions-declarations-commitment-machine`; [ART-FLORES] `if-you-never-do-anything-else-flores-committed-speech`; [ART-NUDGE] `nudging-with-questions-the-socratic-gate`; [ART-PHYS] `the-physics-of-socratic-prompting`; [ART-LAY] `giving-ai-scars-layperson-guide`; [ART-3AM] `why-ai-coding-agents-crash-at-3am`
- Papers under `/papers/`: [P-ABS] `abstract`; [P-TAX] `the-synthetic-scar-taxonomy-linguistic-mechanics`; [P-ARCH] `closed-loop-multi-agent-architecture`; [P-EMP] `empirical-methodology-metrics`; [P-REL] `related-work`; [P-DISC] `discussion-epistemology-boundaries-strategic-maturity`; [P-FURTHER] `further-study-independent-experimental-replication-protocols`; [P-CASES] `case-studies`
- Theory under `/theory/`, tag = chapter and section: [T1-12], [T1-30] = `chapter-1-physical-foundations`; [T2-37], [T2-43], [T2-45], [T2-48] = `chapter-2-speech-acts`; [T3-68], [T3-77], [T3-86] = `chapter-3-declarative-dependency-graphs`; [T5-126], [T5-132], [T5-149] = `chapter-5-multi-agent-orchestration`; [T6-154], [T6-180] = `chapter-6-socratic-prompting`; [T8-215], [T8-222], [T8-223], [T8-224], [T8-231], [T8-234] = `chapter-8-invariants-dijkstra-barriers`; [T9-258], [T9-259], [T9-262], [T9-266], [T9-269] = `chapter-9-empirical-methodology`; [T10-272], [T10-287] = `chapter-10-test-driven-development`; [T11-291] = `chapter-11-software-evolution`; [T12-320], [T12-325], [T12-338.11], [T12-343] = `chapter-12-open-developer-commons`. (Chapter 4, 7 and most of 10 and 12 not read beyond headings.)

Author's gist: [GIST] https://gist.github.com/RandalSchwartz/598a1f2b5760a23c1d70b57e9e85393f ; [GIST-SKILL] its `SKILL.md`; [GIST-GLOBAL] its `GLOBAL_RULES.md`; [GIST-README] `000 README.md`; [GIST-VACCINE] `09_failure_vaccines.md`.

Third party (read via `gh api`, read-only)
- [BLUR] https://github.com/cppalliance/tools-public/blob/master/lessons/semantic-blur-effect.md
- [DOK] https://github.com/cppalliance/tools-public/blob/master/writing/dokuman.md
- [MINER] https://github.com/slabbdev/scar-miner (README, gates.py, omission.py)
- [AGOPS] https://github.com/FranciscoGrandon/ag-ops (README only)
- [HEDGES] https://github.com/hedges333/makefile-workflow-skill (README only)
- [NEMIK] https://github.com/mikemol/nemik (README only; per-repo queue file plus ledger plus global events log, built on OSLC/PROV/SHACL)
- [NOIREBOX] https://github.com/noirebox/noirebox (README head only)

## 8. What I skipped or only skimmed

- Theory chapters 4, 6, 7, 10, 11, 12: read only the sections named above; the remainder (about 250 sections) are Dart/Flutter ticket analyses and generic "physics" elaborations. Heading lists for all chapters are in `theory_headings.txt` (alongside this report).
- Case studies (155, 643 KB): headings only.
- Gist reference files `01` to `10` and `09_*` modules: saved in `pages/gist_*`, not read in full.
- DEV.to originals and comment threads (the site republishes the articles but quotes comments); the cited external papers (Razeghi et al., Kandpal et al., Lost-in-the-Middle) were not fetched.
- `RandalSchwartz/Scars` and `vinniefalco/scars`: 404. `cppalliance/tools-public` other lessons, `noirebox` docs, `mtools`/`pathsforward`, `dart-sdk-skills`: not fetched.
- Page count is below your 60-page threshold for the site itself (7 index pages + 13 articles + 13 paper chapters + 12 theory chapters = 45), so nothing was dropped for volume; the skips above are depth skips inside very large pages.

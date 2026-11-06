Paths are relative to the Batbox repository root, and all line numbers were checked by reading the files. The binary was not run and the two forbidden files were not opened.

Naming: in this codebase "poetry" means prompt, "Flight" means session, "Gadget" means tool, "Roost" means the project directory, and "stash" means cache (`docs/vocabulary.md`). Most of the code is a C++ port of a Node codebase, so prompts and logic look foreign and some paths are dead (see section 6).

## 1. Compaction

**Trigger**
- **Settings:** the threshold is `compactAt`, default 100000 (`src/application/Config.hpp:54`). It is set by `--compact-at` (`Config.cpp:273`), `BATBOX_COMPACT_AT` (`:206`), `compact_at` in native-client.json (`:59`), or `/context` and `/config` (`application_compaction_settings/Policy.cpp`). `autoCompactEnabled` is a separate toggle.
- **Conversion to a percent:** `application_engine/AutoPolicy.hpp:39-51` turns `compactAt` into a `BATBOX_AUTOCOMPACT_PCT_OVERRIDE` percentage of the effective window.
- **Window math:** `services/compact/autoCompact.cpp:11-33`.
  - Effective window = contextWindow − min(maxOutput, 20000).
  - Threshold = min(pct × effective, effective − 13000).
  - Default window is 280000, so the default threshold lands at 100000.
- **Other environment overrides:** `BATBOX_AUTO_COMPACT_WINDOW`, `BATBOX_BLOCKING_LIMIT_OVERRIDE` and `BATBOX_DISABLE_COMPACT` / `BATBOX_DISABLE_AUTO_COMPACT` (`:53-57`).
- **Token count:** `utils/tokens.cpp:96-119`.
  - It takes the last API response's usage plus a rough estimate of the messages after it.
  - If there is no usage, it estimates everything.
  - The estimate is about 4 bytes per token, with images and documents counted as 2000 (`services/tokenEstimation.hpp:7`, `application_model/Microcompact.cpp:57-81`).
- **Where it runs:** the per-turn query setup. The order is messages after the last boundary, then the tool-result budget, then snip (off), then microcompact, then collapse (off), then autocompact (`query/AsyncSetupStart.cpp:85-232`).
  - `application_engine/CompactModel.cpp:104-127` calls `shouldAutoCompact` (`compaction/Auto.cpp:4-14`).
  - That function skips the sources `"compact"` and `"session_memory"`, and nothing else.
  - After 3 consecutive failures it stops retrying (`Auto.cpp:21`, `CompactModel.cpp:115`).
- **Hard stop:** if auto-compact is off, or it did not help, and tokens ≥ effective − 3000, the turn ends with `blocking_limit` and a "prompt too long" error (`query/AsyncSetupFinish.cpp:~76-92`).

**Prompt** (`compaction/poetryEntries/Base.cpp:4-99`, wrapped by `Poetry.cpp:7-13` and `Boundary.cpp:3-13`)
- It starts with `CRITICAL: Respond with TEXT ONLY. Do NOT call any tools.`
- Then comes the body: "Your task is to create a detailed summary of the conversation so far, paying close attention to the user's explicit requests and your previous actions… essential for continuing development work without losing context."
- The model first writes an `<analysis>` block, then a `<summary>` with 9 fixed sections:
  1. Primary Request and Intent
  2. Key Technical Concepts
  3. Files and Code Sections
  4. Errors and fixes
  5. Problem Solving
  6. All user messages
  7. Pending Tasks
  8. Current Work
  9. Optional Next Step
- The body ends with "There may be additional summarization instructions… `## Compact Instructions`" and examples that mention "typescript code changes". These examples, and the whole template, are coding-specific.
- Extra instructions are appended as `Additional Instructions:`. They come from the `/compact <text>` argument merged with PreCompact hook stdout (`Helpers.cpp:91-96`, `Workflow.cpp:43-45`).
- A `REMINDER: Do NOT call any tools…` trailer closes it.

**Who runs the summary**
- **Fork path (tried first):** a one-turn forked Bat with `maxTurns=1` and `querySource="compact"` that reuses the parent's system prompt, user context and messages. This keeps the provider's prompt cache valid (`compaction/Summary.cpp:7-20`, `application_engine/CompactFork.cpp:100-181`, enable flag default true at `CompactFork.hpp:15`). All tools are denied (`Workflow.cpp:28-31`).
- **Fallback:** a direct streaming call, with a different system prompt: `"You are a helpful AI assistant tasked with summarizing conversations."` (`Summary.cpp:46`).
  - Only the Read gadget is offered.
  - Thinking is disabled and output is capped at min(20000, model max).
  - Images become `[image]` or `[document]` (`Helpers.cpp:7-36`).
  - It retries once on failure (`Summary.cpp:21`, `CompactBinding.cpp:182`).
- **Prompt-too-long retries:** up to 3 times, dropping the oldest API-round groups (about 20%, or by the token gap) and prepending the marker `[earlier conversation truncated for compaction retry]` (`Workflow.cpp:55-68`, `Helpers.cpp:47-71`).

**Verbatim versus summarised**
- Nothing is kept verbatim. The post-compact list is exactly boundary, summary, attachments, then SessionStart hook messages (`Workflow.cpp:97-110`, `Helpers.cpp:72-79`).
- The summary message text is built at `Poetry.cpp:58-77`.
  - It begins: "This session is being continued from a previous conversation that ran out of context. The summary below covers the earlier portion…"
  - `<analysis>` is stripped (`formatCompactSummary`, `Poetry.cpp:35-57`).
  - It adds "If you need specific details from before compaction… read the full transcript at: <flight .jsonl path>".
  - It ends with "Continue the conversation from where it left off without asking the user any further questions…"
- Re-injected as attachments, not as the original messages (`compaction/Restore.cpp`):
  - the 5 most recently read files, each up to 5000 tokens, 50000 total (`:92-122`);
  - invoked skills, 5000 tokens each and 25000 total (`:51-67`);
  - the plan file and plan-mode reminder (`:45-50, 68-75`);
  - status of async Bat Missions (`:76-91`).
- Memory-file paths are excluded from the file restore (`:35-44`).
- The read-file cache and loaded nested-memory paths are cleared (`Workflow.cpp:71-73`).

**Where the summary lives**
- It is a user message with `isCompactSummary=true`, in the in-memory list and in the Flight JSONL.
- The boundary is a `system/compact_boundary` message with `compactMetadata{trigger,preTokens,preCompactDiscoveredTools}` and `logicalParentUuid` (`messages/Context.cpp:19-28`).
- Chain handling: the boundary is written with `parentUuid=null` (`flights/async_project/MessageChain.cpp:80`), and later loads ignore everything before the last boundary (`messages/Context.cpp:42-46`).
- Metadata is re-appended to the tail after compaction (`Workflow.cpp:113`).
- Post-compact cleanup (`CompactBinding.cpp:77-103`, `compaction/Cleanup.cpp`) does the following:
  - clears the user-context stash, system-prompt sections and message stash;
  - resets the memory-file discovery stash (`Provider::resetProjectInstructionsAfterCompact`, `application_poetry/ProviderCore.cpp:144`);
  - calls `platform::releaseFreeMemory()` (`Compactor.cpp:16-21`).
- The engine then re-reads BATBOX.md, the date and the project recovery packet (`application_engine/Defaults.cpp:186-206`).

**Tool results and Gadget outputs**
- **Per-call persistence (active):**
  - `application_engine/Execution.cpp:68-80` calls `gadget_results`.
  - Results above the gadget's `maxResultSizeChars` are written to `<state>/projects/<proj>/<flightId>/tool-results/<id>.txt|json` (`gadget_results/Storage.cpp:6-18`).
  - The model sees `<persisted-output>…Output too large… Full output saved to: <path>… Preview (first 2.0KB)` (`Helpers.cpp:31-38`).
  - Default cap is 50000 chars (`Storage.hpp:15`), Bash is 30000 (`gadgets/bash/BashGadget.cpp:161`), and MCP, Bat and Skill gadgets are 100000.
- **Per-message budget (dormant):** `MAX_GADGET_RESULTS_PER_MESSAGE_CHARS=200000` exists. The live host never provisions the content-replacement state it needs, so it is a no-op (commit `2efcbf9`, `query/AsyncSetupStart.cpp:109-123`).
- **Time-based microcompact (off by default):** `application_model/Microcompact.cpp:83-154`.
  - Settings: `timeBasedMicrocompact` (bool), `timeBasedMicrocompactGapMinutes` (default 60) and `timeBasedMicrocompactKeepRecent` (default 5), read at `application/ModelBinding.cpp:101-112`.
  - After the gap, it replaces old results from Read, Bash, PowerShell, Grep, Glob, WebSearch, WebFetch, Edit and Write with `[Old tool result content cleared]`.
  - The system prompt warns the model: "write down any important information you might need later… the original tool result may be cleared later" (`poetryEntries/AsyncSystemPoetryInternal.hpp:32-34`).

**Manual `/compact`**
- It tries the (stubbed) Flight-memory path, then microcompacts, then runs the full workflow with the `/compact <text>` argument as custom instructions (`commands_indigenous/Compact.cpp:64-147`, `application_commands/Commands.cpp:277-345`).
- The result is applied at `user_input/SlashLocal.cpp:65`.

**Bats (sub-agents)**
- Bats use the same query loop, with source `agent:*`. Autocompact has no Bat guard (`Auto.cpp:6`), and Bat transcripts record `compact_boundary` (`bats/RunBatHelpers.cpp:9-13`).
- Bat context is derived from the parent's stash-safe snapshot (`bats/SubagentContext.cpp`). Bats can omit BATBOX.md from their user context via the definition's `omitBatboxMd`, and Explore and Plan Bats drop `gitStatus` (`bats/RunBatPreflight.cpp:67-74`).
- Some cleanup steps run only for the main thread (`Cleanup.cpp:5-9`).
- Running Bat Missions are re-announced after the parent compacts (`Restore.cpp:76-91`).
- I did not trace a Bat autocompact end to end.

**With `--resume`**
- Loading is bounded by the last boundary. Above 5 MiB (`LoadServices::skipThreshold`), only the post-boundary tail is parsed, and pre-boundary metadata is scanned separately (`flights/Load.cpp:29-47`, `Load.hpp:7`).
- A `preservedSegment` relink exists (`flights/Relink.cpp:2-60`) but only the unused partial path produces one.
- Summaries are not re-run on resume.

**"Memory optimization" in the Nov 2026 overhaul**
- This was process-RSS work, not LLM-context work: branch `feat/memory-perf`, merge `96daaa2`.
  - `bd978a7`: a lower resume-load peak (chunked read, one-line-at-a-time parse).
  - `2efcbf9`: one conversation copy in the stash-safe snapshot.
  - `3535760`: a slim permission-callback context.
  - `3f84e2f`: `/status` resident and heap figures, plus `platform::releaseFreeMemory`.
  - `c4fcf3c`: incremental transcript rows.
- There is no prune, trim or budget logic for LLM context beyond what is listed above.

## 2. Project init and instructions

**There is no AGENTS.md or CLAUDE.md support**, and `/init` is not a registered command. The only mention is a help-section label (`commands_indigenous/HelpModel.cpp:141`).

**Instruction files** (loaded by `instructions/project/*`, driven by `application_poetry/ProviderContext.cpp:27-118`). The order is:
1. Managed: `<state>/instructions/managed/BATBOX.md` and `rules/`.
2. User: `~/.batbox/BATBOX.md` and `~/.batbox/rules/`.
3. For each ancestor of the cwd, root first (`instructions/project/ProjectDiscovery.cpp:50-82`, async mirror in `async/MemoryStages.cpp:15-85`): `<dir>/BATBOX.md`, `<dir>/.batbox/BATBOX.md`, `<dir>/.batbox/rules/*.md`, then `<dir>/BATBOX.local.md`.
4. Additional directories (`:83-94`).
5. The auto-memory `MEMORY.md` (type `AutoMem`, `:95-100`).
6. Team memory, which is disabled (`ProviderContext.cpp:99,115`).

**Walk and parsing rules**
- **Walk rules:** the walk goes to the filesystem root, not just the git root. Git matters only for nested-worktree dedup (`ProjectDiscovery.cpp:55-70`).
- **Include syntax:** `@path` includes in markdown are supported, with depth < 5 (`ProjectFiles.cpp:58`), a text-extension whitelist (`ProjectContent.cpp:10-20`), and an external-include approval setting `allowExternalInstructions` (`ProviderContext.cpp:79`).
- **Comments:** HTML comments are stripped.
- **Conditional rules:** `paths:` frontmatter globs load when matching files are touched, via the nested-memory attachments (`attachment_triggers/NestedMemory.cpp`, `instructions/project/async/ConditionalRules.cpp`).
- **Settings:** the keys `settingSources.{user,project,local}Settings`, `skipProjectInstructions`, `bareMode`, `memoryExcludePatterns` and the env `BATBOX_DISABLE_BATBOX_MDS` gate loading (`ProviderContext.cpp:56,73,109-113`, `context/AsyncUserContext.cpp:31-38`).

**Size limits**
- For `MEMORY.md` only: 200 lines or 25000 bytes, with a WARNING footer (`instructions/MemoryTruncation.cpp:6-45`, applied at `ProjectContent.cpp:47-48`).
- For BATBOX.md there is no hard cap. `getLargeMemoryFiles` (over 40000 UTF-16 units, `ProjectContext.cpp:16-22`) has no callers.

**Caching**
- `MemoryDiscoveryStash` caches the file list, errors included, until cleared (`ProjectDiscovery.cpp:24-30, 132-140`).
- `refreshMemoryFilesIfChanged` compares mtime+size stamps of the watched files (`ProviderCore.cpp:115-132`). It runs on every `userContext`, `memoryFiles` and `systemPoetry` call, and `observeFileChange` (`application_memory/Provider.cpp:66`) is invoked by post-Gadget-use hooks (`HostBackend.cpp:279-286`). On change it clears the instructions and the prompt sections.

**The Roost "init" is `.batbox/` recovery files, not an instructions file**
- `/project [status|init|reload]` (`application_commands/ProjectCommand.cpp`) creates `.batbox/Makefile`, `00_project.md` and `10_memory.md` from templates (`project_recovery/Templates.cpp`, `Init.cpp:63-75`). It is explicit opt-in, at the nearest `.git` root or the cwd.
- At runtime, `project_recovery::load` (`Load.cpp:81-117`) reads those three files, 2 KiB each within an 8 KiB packet (`Recovery.hpp:9-12`). It adds an index of up to 32 `NN_*.md` topic filenames, in a 1 KiB index, and never evaluates Makefile recipes.
- Discovery (`Discovery.cpp:39-55`) walks up from the cwd until a `.git` entry, requiring at least one of the three files.
- The packet is sent as `userContext["projectRecovery"]` (`ProviderCore.cpp:60-85`, `ProviderRecovery.cpp:23-76`), re-stamped by directory and file metadata.

**Not a code project** (no git and no init files)
- Instructions come from the user file only, plus any ancestor BATBOX.md.
- `gitStatus` is skipped (`context/AsyncSystemContext.cpp:20-23`) and the environment block says non-git.
- `projectRecovery` is empty.
- Auto-memory is still on by default: it needs only `autoMemoryEnabled != false`, no `BATBOX_SIMPLE` and no `BATBOX_DISABLE_AUTO_MEMORY` (`instructions/MemoryPath.cpp:30-40`). Its path falls back to the cwd (`:89-96`).

## 3. Persistent memory and sessions

**Layout**
- `~/.batbox/` (`BATBOX_CONFIG_DIR` overrides) holds `native-client.json`, `settings.json`, `agents/*.md`, `skills/`, `commands/` (legacy skills), `mcp.json`, `BATBOX.md`, `rules/` and `lsp.json`. See `application/Config.cpp:167-185` and `docs/indigenous-configuration.md`.
- `stateDirectory` is `~/.batbox/state` (`Config.cpp:175`). It is the legacy `~/.local/state/batbox-native` if that exists and `BATBOX_CONFIG_DIR` is unset, or `state_directory` from the client config.
- Under the state directory:
  - `projects/<sanitized cwd>/<flightId>.jsonl` is the transcript. Sanitization maps every non-alphanumeric character to `-`, truncates, and appends a hash for long paths (`flights/Paths.cpp:12-17`, `runtime/PathSanitization.cpp`).
  - `projects/<proj>/<flightId>/subagents/agent-<id>.jsonl` and `.meta.json` are the Bat transcripts (`Paths.cpp:28-38`).
  - `projects/<proj>/<flightId>/tool-results/` holds persisted Gadget outputs.
  - `projects/<canonical git root or cwd>/memory/` is auto-memory (`MemoryPath.cpp:84-102`).
  - `plans/`, `tasks/<id>.inputs.jsonl`, `image-cache/<flight>` and `tmp/`.
- Per project, in the repo:
  - `.batbox/{settings.json,settings.local.json,agents/,rules/,BATBOX.md,Makefile,00_project.md,10_memory.md,agent-memory/<bat>/,agent-memory-local/<bat>/,lsp.json,worktrees/}` (`settings/Paths.cpp:69-70`, `memory_prefetch/BatMemoryPath.cpp:16-40`).
  - User-scope Bat memory is `<state>/agent-memory/<bat>/`.

**The file-memory feature (the closest thing to cross-Flight memory)**
- **Format:** `MEMORY.md` is a one-line-per-entry index. Topic files have frontmatter `name`, `description` and `type` (`instructions/MemoryLines.cpp:40-80`). The four types are user, feedback, project and reference (`MemoryTypesIndividual.cpp`).
- **Write path:** the model writes the files itself with Write and Edit. There is no memory database (`application_memory/Provider.hpp:28-29`).
- **Prompt:** how to save memory is injected as the `memory` section (`MemoryLoad.cpp:75-97`). The `MEMORY.md` contents arrive separately through userContext.
- **Recall:** per user turn, a side query using the default model (`memory_prefetch/Selector.cpp`, querySource `memdir_relevance`) picks up to 5 topic files from a manifest of names and descriptions. They are attached as `relevant_memories` (`Collect.cpp`).
  - Each file is capped at 200 lines or 4096 bytes (`Read.cpp:44-48`).
  - It stops when 60 KiB has already been surfaced (`Start.cpp:71`).
  - It skips only when memory maintenance is running or the input has no whitespace (`Start.cpp:54,69`).
  - Controlled by `memoryRecallEnabled`, default on (`Provider.cpp:33-38`).
- **"Dream" consolidation:** a forked Bat runs a 4-phase prompt (Orient, Gather, Consolidate, Prune and index) over the memory directory (`ConsolidationPoetry.cpp`). It starts from the Stop hook after main-thread turns (`query/StopHooks.cpp:131-135`, `Consolidation.cpp:46-181`).
  - It is off unless `autoDreamEnabled == true`.
  - It needs ≥24h since the last run and ≥5 other Flights touched since (`autoDream.minHours` / `minSessions`).
  - The gate is the mtime of `<memdir>/.consolidate-lock` plus a PID lock (`ConsolidationLock.cpp:11-58`).
  - A manual trigger and status live in the `/memory` panel (`application_commands/MemoryPanel.cpp`).
- **Bat memory:** a Bat definition with `memory: user|project|local` gets a "Persistent Agent Memory" section appended to its system prompt (`application_bats/Memory.cpp:58-94`).

**Transcript format**
- JSONL, one entry per line, append-only (`flights/Metadata.cpp:73-85`).
- **Message entries** are types `user`, `assistant`, `attachment` and `system`, with `uuid`, `parentUuid`, `isSidechain`, `agentId?`, `sessionId`, `cwd`, `version` (hard-coded `"0.0.1"`, `application_flights/Flight.cpp:72`), `gitBranch` and `slug` (`flights/async_project/MessageChain.cpp:80-96`).
- **Metadata entries:** `custom-title`, `ai-title`, `tag`, `agent-name`, `agent-color`, `agent-setting`, `mode`, `worktree-state`, `pr-link`, `summary{leafUuid}` (read-only, nothing writes it), `file-history-snapshot`, `attribution-snapshot`, `content-replacement`, `last-prompt` and `flight-end`.
- Entries are applied at `flights/LoadParsing.cpp:25-76`.
- **Re-appending:** metadata is re-appended after compaction and on clean exit, so the picker's 64 KiB tail read sees it (`Flight.cpp:174-184`, `FlightListing.cpp:18-20`).

**Titles and slugs**
- Title priority is custom (`/rename`), then AI-generated from the first prompt (`auto_title`, default true, `HostFrontend.cpp:446`), then the first prompt (`FlightListing.hpp:19`).
- The slug is a three-word name from the plan-slug generator (`application_flights/Plans.cpp`, `PlanWords.hpp`).

**Resume**
- **CLI:** `-c` and `--continue` call `Restore::latest()`. `-r` opens the picker. `--resume <id|slug|title words>` calls `Restore::find`, matching this directory, then its worktrees, then all projects (`Config.cpp:257-261`, `HostCore.cpp:496-514`, `application_flights/RestoreLoad.cpp:330,359`).
- **What is restored** (`application_flights/RestoreApply.cpp`):
  - the message chain after the last boundary, normalized (`:155-240`):
    - drops unpaired tool_use;
    - drops thinking-only and blank assistant messages;
    - appends "No response requested." when the chain ends in a user turn;
  - the plan file and metadata;
  - file-history snapshots in app state, and the read-file cache rebuilt from Read, Write and Edit tool calls;
  - the model and permission mode (never `bypassPermissions`), plus worktree and Roost rebinding.
- **Not restored:** the system prompt, user context and instructions are rebuilt fresh at each launch.

## 4. Prompt assembly

**Static head**
- The static head is assembled in `poetryEntries/AsyncPoetryFinish.cpp:6-17`: intro, system, doing-tasks (unless the output style drops it), actions, using-your-gadgets, tone and style, then output efficiency.

**Dynamic sections** (`AsyncPoetryDynamic.cpp:8-37`)
- Each section is cached per name until `clearSystemPoetrySections` (`poetryEntries/Sections.cpp:21-55`), except `mcp_instructions`, which is uncached. The order is:
  1. `session_guidance`
  2. `memory`
  3. `ant_model_override`
  4. `env_info_simple`
  5. `language`
  6. `output_style`
  7. `mcp_instructions` (uncached)
  8. `scratchpad`
  9. `frc`
  10. `summarize_tool_results`
- `token_budget`, `numeric_length_anchors` and `brief` are feature-gated.
- `session_guidance` (`GuidanceFlight.cpp:17-95`) carries ask-user hints, `! <cmd>` hints, the Agent gadget section, the Explore guidance and the Skill hint.
- `BATBOX_SIMPLE=1` replaces everything with a 3-line prompt (`AsyncPoetryStart.cpp:7-11`).

**Per-request context**
- **System context** (`{gitStatus}`) is appended to the last system block as `key: value` lines (`application_model/ContextImports.cpp:21-36`).
- **User context** (`{batboxMd, currentDate, projectRecovery}`) is prepended as the first message. It is a meta user message wrapped in `<system-reminder>As you answer the user's questions, you can use the following context: # batboxMd\n…` and ends with "IMPORTANT: this context may or may not be relevant…" (`ContextImports.cpp:37-55`).
- `batboxMd` begins "Codebase and user instructions are shown below. Be sure to adhere to these instructions. IMPORTANT: These instructions OVERRIDE any default behavior and you MUST follow them exactly as written." (`ProjectContext.cpp:7-10, 58-63`). The files follow in the discovery order of section 2, each labelled "(project instructions, checked into the codebase)" or similar. The auto-memory label is "(user's auto-memory, persists across conversations)".

**Skills, Bats and attachments**
- **Skills** are listed in the Skill gadget's description, budgeted at 1% of the context window × 4 chars, default 8000, each description capped at 250 chars (`skills/SkillGadgetPoetry.cpp:11-12, 74-97`).
- **Bat descriptions** come from the Agent gadget's description (`bats/BatGadgetPoetry*.cpp`) and have no budget.
- **Deferred-gadget, Bat-listing and MCP deltas** are stubbed to empty after compaction (`CompactBinding.cpp:173-175`).
- **Attachments** (relevant_memories, nested BATBOX.md and rules, file attachments) are appended per turn.

**Budgeting**
- The only prompt-side budgets are the skill listing, `MEMORY.md` at 200 lines / 25000 bytes, recall at 5 files / 60 KiB and the recovery packet at 8 KiB. There is no overall token budget across these sections.
- `/context` estimates categories (`application_usage/UsageCommand.cpp:225`).

## 5. Extension seams (least disruption first)

1. **Hooks (user-facing, process-based).**
   - `PreCompact` stdout becomes extra compaction instructions, and `PostCompact` receives `compact_summary` (`instructions/hooks/runtime/CompactHooks.cpp:43-105`, bound at `CompactBinding.cpp:116-138`).
   - `SessionStart` with source `compact` runs after compaction (`:139-161`). Its additional context is injected as messages.
   - `Stop` runs after every main-thread turn, and `InstructionsLoaded`, `SubagentStart` (adds a `hook_additional_context` attachment) and `FileChanged` also exist (`settings/schema/imports/HooksDescriptor.cpp`).
2. **A native memory layer** can mirror `application_memory::Provider` (`application_memory/Provider.hpp`).
   - Startup loader: the discovery stash and `getMemoryFiles` (`ProviderContext.cpp:92-100`), where a new type next to `AutoMem` would flow into `batboxMd`.
   - Prefetch seam: `engine.startMemoryPrefetch` (`application_engine/Defaults.cpp:~411`), a per-turn attachment injector.
   - Post-turn: `stop.executeAutoDream` (`Defaults.cpp:311`), i.e. a native post-turn hook with the full stopContext.
   - Prompt section: `service.loadMemoryPoetry` (`ProviderPoetry.cpp:194-209`) feeds the cached `memory` section.
   - File-change observation: `observeFileChange` / `refresh` (`application_memory/Provider.cpp:60-83`).
3. **Compaction seams.**
   - `tryFlightMemoryCompaction` is already plumbed in `AutoServices` and in the `/compact` command (`Auto.cpp:29`, `Compact.cpp:72`), but is stubbed to return nothing at `CompactModel.cpp:87` and `Commands.cpp:296`. It is a ready replace-the-summary hook.
   - Others: `executePreCompactHooks`, `executePostCompactHooks`, `getCompactPoetry(custom)`, `createPostCompactFileAttachments` and `refreshUserContextAfterCompact`.
   - The owner is `application_engine/CompactBinding.cpp` (the service graph), wired up in `application/HostBackend.cpp:188`.
4. **Post-sampling hooks:** `engine.postSamplingHooks` is an empty default at the end of `Defaults.cpp`.
5. **Session persistence:** `reAppendFlightMetadata` (`Workflow.cpp:113`), `flights/MetadataOperations.hpp` (add a new metadata entry type, applied at `LoadParsing.cpp:31-40`), and the markEnded path at `HostShutdown.cpp:39`.

## 6. Known gaps, TODOs and dead paths

- **Recall and dream:** `autoDreamEnabled` is off by default, and the dream scan counts transcripts only inside the same project directory.
- **Daily-log and past-context sections:** `dailyLogFeature`, `searchPastContextEnabled`, and the "Searching past context" section are never set by the provider (`ProviderPoetry.cpp:194-209`).
- **Features that hard-fail at startup if enabled** (`application_engine/Defaults.cpp:155-163`): `EXPERIMENTAL_SKILL_SEARCH`, `TOKEN_BUDGET`, `BG_SESSIONS`, `CHICAGO_MCP`, `TEMPLATES`, `EXTRACT_MEMORIES`, `CONTEXT_COLLAPSE`, `REACTIVE_COMPACT`, `HISTORY_SNIP` and `CACHED_MICROCOMPACT`. `resetContextCollapse` throws `logic_error` if reached (`CompactBinding.cpp:82-84`).
- **Dead code:** `partialCompactConversation` (`compaction/Partial.cpp:17`) has no callers, so there is no way to keep recent messages verbatim and compact only the older ones. Nearby stubs: `markPostCompaction`, `clearClassifierApprovals`, `clearSpeculativeChecks` and `clearLastSummarizedMessageId` are no-ops.
- **Summary prompt:** the prompt is coding-only, with no hook to swap the template other than `Additional Instructions` (`Poetry.cpp:7-13`).
- **Instructions:** `getLargeMemoryFiles` is unused, so there is no BATBOX.md size guard. Instructions are never loaded from AGENTS.md or CLAUDE.md, and the `/init` help entry has no command behind it.
- **Roadmap:** `ROADMAP.md` only lists "Better Flight recovery" and exercising compaction and interrupted Flights against multiple backends. It has no memory item.
- **Docs mismatch:** `docs/everyday-use.md:91-94` promises saved notes survive "restart and compaction". That holds for the file memory and `.batbox/` files, but nothing carries session-only facts across a compaction except the summary prompt.
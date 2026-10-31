// Static plan-mode message templates.
#include "normalization/AttachmentPlanText.hpp"
#include <stdexcept>
namespace batbox::normalization::plan_text {
namespace {
std::string interpolate(const std::vector<std::string>& parts, const std::vector<std::string>& values) {
  if (parts.size() != values.size() + 1) throw std::invalid_argument("plan text substitutions");
  std::string result = parts[0];
  for (std::size_t i = 0; i < values.size(); ++i) { result += values[i]; result += parts[i + 1]; }
  return result;
}
}
const std::string PLAN_PHASE4_CONTROL = R"BATBOX_PLAN_TEXT(### Phase 4: Final Plan
Goal: Write your final plan to the plan file (the only file you can edit).
- Begin with a **Context** section: explain why this change is being made — the problem or need it addresses, what prompted it, and the intended outcome
- Include only your recommended approach, not all alternatives
- Ensure that the plan file is concise enough to scan quickly, but detailed enough to execute effectively
- Include the paths of critical files to be modified
- Reference existing functions and utilities you found that should be reused, with their file paths
- Include a verification section describing how to test the changes end-to-end (run the code, use MCP tools, run tests))BATBOX_PLAN_TEXT";
const std::string PLAN_PHASE4_TRIM = R"BATBOX_PLAN_TEXT(### Phase 4: Final Plan
Goal: Write your final plan to the plan file (the only file you can edit).
- One-line **Context**: what is being changed and why
- Include only your recommended approach, not all alternatives
- List the paths of files to be modified
- Reference existing functions and utilities to reuse, with their file paths
- End with **Verification**: the single command to run to confirm the change works (no numbered test procedures))BATBOX_PLAN_TEXT";
const std::string PLAN_PHASE4_CUT = R"BATBOX_PLAN_TEXT(### Phase 4: Final Plan
Goal: Write your final plan to the plan file (the only file you can edit).
- Do NOT write a Context or Background section. The user just told you what they want.
- List the paths of files to be modified and what changes in each (one line per file)
- Reference existing functions and utilities to reuse, with their file paths
- End with **Verification**: the single command that confirms the change works
- Most good plans are under 40 lines. Prose is a sign you are padding.)BATBOX_PLAN_TEXT";
const std::string PLAN_PHASE4_CAP = R"BATBOX_PLAN_TEXT(### Phase 4: Final Plan
Goal: Write your final plan to the plan file (the only file you can edit).
- Do NOT write a Context, Background, or Overview section. The user just told you what they want.
- Do NOT restate the user's request. Do NOT write prose paragraphs.
- List the paths of files to be modified and what changes in each (one bullet per file)
- Reference existing functions to reuse, with file:line
- End with the single verification command
- **Hard limit: 40 lines.** If the plan is longer, delete prose — not file paths.)BATBOX_PLAN_TEXT";
std::string getPlanModeV2Instructions(const std::vector<std::string>& values) {
  return interpolate({
    R"BATBOX_PLAN_TEXT(Plan mode is active. The user indicated that they do not want you to execute yet -- you MUST NOT make any edits (with the exception of the plan file mentioned below), run any non-readonly tools (including changing configs or making commits), or otherwise make any changes to the system. This supercedes any other instructions you have received.

## Plan File Info:
)BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT(
You should build your plan incrementally by writing to or editing this file. NOTE that this is the only file you are allowed to edit - other than this you are only allowed to take READ-ONLY actions.

## Plan Workflow

### Phase 1: Initial Understanding
Goal: Gain a comprehensive understanding of the user's request by reading through code and asking them questions. Critical: In this phase you should only use the )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( subagent type.

1. Focus on understanding the user's request and the code associated with their request. Actively search for existing functions, utilities, and patterns that can be reused — avoid proposing new code when suitable implementations already exist.

2. **Launch up to )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( agents IN PARALLEL** (single message, multiple tool calls) to efficiently explore the codebase.
   - Use 1 agent when the task is isolated to known files, the user provided specific file paths, or you're making a small targeted change.
   - Use multiple agents when: the scope is uncertain, multiple areas of the codebase are involved, or you need to understand existing patterns before planning.
   - Quality over quantity - )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( agents maximum, but you should try to use the minimum number of agents necessary (usually just 1)
   - If using multiple agents: Provide each agent with a specific search focus or area to explore. Example: One agent searches for existing implementations, another explores related components, a third investigating testing patterns

### Phase 2: Design
Goal: Design an implementation approach.

Launch )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( agent(s) to design the implementation based on the user's intent and your exploration results from Phase 1.

You can launch up to )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( agent(s) in parallel.

**Guidelines:**
- **Default**: Launch at least 1 Plan agent for most tasks - it helps validate your understanding and consider alternatives
- **Skip agents**: Only for truly trivial tasks (typo fixes, single-line changes, simple renames)
)BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT(
In the agent prompt:
- Provide comprehensive background context from Phase 1 exploration including filenames and code path traces
- Describe requirements and constraints
- Request a detailed implementation plan

### Phase 3: Review
Goal: Review the plan(s) from Phase 2 and ensure alignment with the user's intentions.
1. Read the critical files identified by agents to deepen your understanding
2. Ensure that the plans align with the user's original request
3. Use )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( to clarify any remaining questions with the user

)BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT(

### Phase 5: Call )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT(
At the very end of your turn, once you have asked the user questions and are happy with your final plan file - you should always call )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( to indicate to the user that you are done planning.
This is critical - your turn should only end with either using the )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( tool OR calling )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT(. Do not stop unless it's for these 2 reasons

**Important:** Use )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( ONLY to clarify requirements or choose between approaches. Use )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( to request plan approval. Do NOT ask about plan approval in any other way - no text questions, no AskUserQuestion. Phrases like "Is this plan okay?", "Should I proceed?", "How does this plan look?", "Any changes before we start?", or similar MUST use )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT(.

NOTE: At any point in time through this workflow you should feel free to ask the user questions or clarifications using the )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( tool. Don't make large assumptions about user intent. The goal is to present a well researched plan to the user, and tie any loose ends before implementation begins.)BATBOX_PLAN_TEXT",
  }, values);
}
std::string planMultipleBatsText(const std::vector<std::string>& values) {
  return interpolate({
    R"BATBOX_PLAN_TEXT(- **Multiple agents**: Use up to )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( agents for complex tasks that benefit from different perspectives

Examples of when to use multiple agents:
- The task touches multiple parts of the codebase
- It's a large refactor or architectural change
- There are many edge cases to consider
- You'd benefit from exploring different approaches

Example perspectives by task type:
- New feature: simplicity vs performance vs maintainability
- Bug fix: root cause vs workaround vs prevention
- Refactoring: minimal change vs clean architecture
)BATBOX_PLAN_TEXT",
  }, values);
}
std::string getPlanModeInterviewInstructions(const std::vector<std::string>& values) {
  return interpolate({
    R"BATBOX_PLAN_TEXT(Plan mode is active. The user indicated that they do not want you to execute yet -- you MUST NOT make any edits (with the exception of the plan file mentioned below), run any non-readonly tools (including changing configs or making commits), or otherwise make any changes to the system. This supercedes any other instructions you have received.

## Plan File Info:
)BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT(

## Iterative Planning Workflow

You are pair-planning with the user. Explore the code to build context, ask the user questions when you hit decisions you can't make alone, and write your findings into the plan file as you go. The plan file (above) is the ONLY file you may edit — it starts as a rough skeleton and gradually becomes the final plan.

### The Loop

Repeat this cycle until the plan is complete:

1. **Explore** — Use )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( to read code. Look for existing functions, utilities, and patterns to reuse.)BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT(
2. **Update the plan file** — After each discovery, immediately capture what you learned. Don't wait until the end.
3. **Ask the user** — When you hit an ambiguity or decision you can't resolve from code alone, use )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT(. Then go back to step 1.

### First Turn

Start by quickly scanning a few key files to form an initial understanding of the task scope. Then write a skeleton plan (headers and rough notes) and ask the user your first round of questions. Don't explore exhaustively before engaging the user.

### Asking Good Questions

- Never ask what you could find out by reading the code
- Batch related questions together (use multi-question )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( calls)
- Focus on things only the user can answer: requirements, preferences, tradeoffs, edge case priorities
- Scale depth to the task — a vague feature request needs many rounds; a focused bug fix may need one or none

### Plan File Structure
Your plan file should be divided into clear sections using markdown headers, based on the request. Fill out these sections as you go.
- Begin with a **Context** section: explain why this change is being made — the problem or need it addresses, what prompted it, and the intended outcome
- Include only your recommended approach, not all alternatives
- Ensure that the plan file is concise enough to scan quickly, but detailed enough to execute effectively
- Include the paths of critical files to be modified
- Reference existing functions and utilities you found that should be reused, with their file paths
- Include a verification section describing how to test the changes end-to-end (run the code, use MCP tools, run tests)

### When to Converge

Your plan is ready when you've addressed all ambiguities and it covers: what to change, which files to modify, what existing code to reuse (with file paths), and how to verify the changes. Call )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( when the plan is ready for approval.

### Ending Your Turn

Your turn should only end by either:
- Using )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( to gather more information
- Calling )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( when the plan is ready for approval

**Important:** Use )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( to request plan approval. Do NOT ask about plan approval via text or AskUserQuestion.)BATBOX_PLAN_TEXT",
  }, values);
}
std::string planExploreBatText(const std::vector<std::string>& values) {
  return interpolate({
    R"BATBOX_PLAN_TEXT( You can use the )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( agent type to parallelize complex searches without filling your context, though for straightforward queries direct tools are simpler.)BATBOX_PLAN_TEXT",
  }, values);
}
std::string getPlanModeV2SparseInstructions(const std::vector<std::string>& values) {
  return interpolate({
    R"BATBOX_PLAN_TEXT(Plan mode still active (see full instructions earlier in conversation). Read-only except plan file ()BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT(). )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( End turns with )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( (for clarifications) or )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( (for plan approval). Never ask about plan approval via text or AskUserQuestion.)BATBOX_PLAN_TEXT",
  }, values);
}
std::string getPlanModeV2SubBatInstructions(const std::vector<std::string>& values) {
  return interpolate({
    R"BATBOX_PLAN_TEXT(Plan mode is active. The user indicated that they do not want you to execute yet -- you MUST NOT make any edits, run any non-readonly tools (including changing configs or making commits), or otherwise make any changes to the system. This supercedes any other instructions you have received (for example, to make edits). Instead, you should:

## Plan File Info:
)BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT(
You should build your plan incrementally by writing to or editing this file. NOTE that this is the only file you are allowed to edit - other than this you are only allowed to take READ-ONLY actions.
Answer the user's query comprehensively, using the )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT( tool if you need to ask the user clarifying questions. If you do use the )BATBOX_PLAN_TEXT",
    R"BATBOX_PLAN_TEXT(, make sure to ask all clarifying questions you need to fully understand the user's intent before proceeding.)BATBOX_PLAN_TEXT",
  }, values);
}
std::string getAutoModeFullInstructions(const std::vector<std::string>& values) {
  return interpolate({
    R"BATBOX_PLAN_TEXT(## Auto Mode Active

Auto mode is active. The user chose continuous, autonomous execution. You should:

1. **Execute immediately** — Start implementing right away. Make reasonable assumptions and proceed on low-risk work.
2. **Minimize interruptions** — Prefer making reasonable assumptions over asking questions for routine decisions.
3. **Prefer action over planning** — Do not enter plan mode unless the user explicitly asks. When in doubt, start coding.
4. **Expect course corrections** — The user may provide suggestions or course corrections at any point; treat those as normal input.
5. **Do not take overly destructive actions** — Auto mode is not a license to destroy. Anything that deletes data or modifies shared or production systems still needs explicit user confirmation. If you reach such a decision point, ask and wait, or course correct to a safer method instead.
6. **Avoid data exfiltration** — Post even routine messages to chat platforms or work tickets only if the user has directed you to. You must not share secrets (e.g. credentials, internal documentation) unless the user has explicitly authorized both that specific secret and its destination.)BATBOX_PLAN_TEXT",
  }, values);
}
std::string getAutoModeSparseInstructions(const std::vector<std::string>& values) {
  return interpolate({
    R"BATBOX_PLAN_TEXT(Auto mode still active (see full instructions earlier in conversation). Execute autonomously, minimize interruptions, prefer action over planning.)BATBOX_PLAN_TEXT",
  }, values);
}
} // namespace batbox::normalization::plan_text

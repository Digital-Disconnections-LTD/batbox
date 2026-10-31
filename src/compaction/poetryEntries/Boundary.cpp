#include "compaction/poetryEntries/Templates.hpp"
namespace batbox::compaction::poetryEntries {
std::string_view preamble() { return R"batbox(CRITICAL: Respond with TEXT ONLY. Do NOT call any tools.

- Do NOT use Read, Bash, Grep, Glob, Edit, Write, or ANY other tool.
- You already have all the context you need in the conversation above.
- Tool calls will be REJECTED and will waste your only turn — you will fail the task.
- Your entire response must be plain text: an <analysis> block followed by a <summary> block.

)batbox"; }
std::string_view trailer() { return R"batbox(

REMINDER: Do NOT call any tools. Respond with plain text only — an <analysis> block followed by a <summary> block. Tool calls will be rejected and you will fail the task.)batbox"; }
}

#include "settings/schema/imports/Hooks.hpp"
namespace batbox::settings::schema::imports {
Json hookDescriptor(){
  static const Json node=value::parse(
    "{\"type\":\"record\",\"value\":{\"type\":\"array\",\"element\":{\"type\":\"object\",\"fields\":[[\"matcher\",{\"type\":\"optional\",\"inner\":{\"type\":\"string\"}}],[\"hooks\",{\"type\":\"array\""
    ",\"element\":{\"type\":\"union\",\"options\":[{\"type\":\"object\",\"fields\":[[\"type\",{\"type\":\"literal\",\"values\":[\"command\"]}],[\"command\",{\"type\":\"string\"}],[\"if\",{\"type\":\"o"
    "ptional\",\"inner\":{\"type\":\"string\"}}],[\"shell\",{\"type\":\"optional\",\"inner\":{\"type\":\"enum\",\"values\":[\"bash\",\"powershell\"]}}],[\"timeout\",{\"type\":\"optional\",\"inner\":"
    "{\"type\":\"number\",\"checks\":[{\"kind\":\"greater_than\",\"value\":0,\"inclusive\":false}]}}],[\"statusMessage\",{\"type\":\"optional\",\"inner\":{\"type\":\"string\"}}],[\"once\",{\"typ"
    "e\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"async\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"asyncRewake\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}"
    "}]],\"passthrough\":false},{\"type\":\"object\",\"fields\":[[\"type\",{\"type\":\"literal\",\"values\":[\"prompt\"]}],[\"prompt\",{\"type\":\"string\"}],[\"if\",{\"type\":\"optional\",\"inner"
    "\":{\"type\":\"string\"}}],[\"timeout\",{\"type\":\"optional\",\"inner\":{\"type\":\"number\",\"checks\":[{\"kind\":\"greater_than\",\"value\":0,\"inclusive\":false}]}}],[\"model\",{\"type\":"
    "\"optional\",\"inner\":{\"type\":\"string\"}}],[\"statusMessage\",{\"type\":\"optional\",\"inner\":{\"type\":\"string\"}}],[\"once\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}]],"
    "\"passthrough\":false},{\"type\":\"object\",\"fields\":[[\"type\",{\"type\":\"literal\",\"values\":[\"agent\"]}],[\"prompt\",{\"type\":\"string\"}],[\"if\",{\"type\":\"optional\",\"inner\":{\"t"
    "ype\":\"string\"}}],[\"timeout\",{\"type\":\"optional\",\"inner\":{\"type\":\"number\",\"checks\":[{\"kind\":\"greater_than\",\"value\":0,\"inclusive\":false}]}}],[\"model\",{\"type\":\"opti"
    "onal\",\"inner\":{\"type\":\"string\"}}],[\"statusMessage\",{\"type\":\"optional\",\"inner\":{\"type\":\"string\"}}],[\"once\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}]],\"pass"
    "through\":false},{\"type\":\"object\",\"fields\":[[\"type\",{\"type\":\"literal\",\"values\":[\"http\"]}],[\"url\",{\"type\":\"string\",\"checks\":[{\"kind\":\"string_format\",\"format\":\"url"
    "\"}]}],[\"if\",{\"type\":\"optional\",\"inner\":{\"type\":\"string\"}}],[\"timeout\",{\"type\":\"optional\",\"inner\":{\"type\":\"number\",\"checks\":[{\"kind\":\"greater_than\",\"value\":0,\"in"
    "clusive\":false}]}}],[\"headers\",{\"type\":\"optional\",\"inner\":{\"type\":\"record\",\"value\":{\"type\":\"string\"},\"key\":{\"type\":\"string\"}}}],[\"allowedEnvVars\",{\"type\":\"optio"
    "nal\",\"inner\":{\"type\":\"array\",\"element\":{\"type\":\"string\"}}}],[\"statusMessage\",{\"type\":\"optional\",\"inner\":{\"type\":\"string\"}}],[\"once\",{\"type\":\"optional\",\"inner\":{"
    "\"type\":\"boolean\"}}]],\"passthrough\":false}],\"discriminator\":\"type\"}}]],\"passthrough\":false}},\"key\":{\"type\":\"union\",\"options\":[{\"type\":\"enum\",\"values\":[\"PreToolUs"
    "e\",\"PostToolUse\",\"PostToolUseFailure\",\"Notification\",\"UserPromptSubmit\",\"SessionStart\",\"SessionEnd\",\"Stop\",\"StopFailure\",\"SubagentStart\",\"SubagentStop\",\"PreComp"
    "act\",\"PostCompact\",\"PermissionRequest\",\"PermissionDenied\",\"Setup\",\"TeammateIdle\",\"TaskCreated\",\"TaskCompleted\",\"Elicitation\",\"ElicitationResult\",\"ConfigChange\","
    "\"WorktreeCreate\",\"WorktreeRemove\",\"InstructionsLoaded\",\"CwdChanged\",\"FileChanged\"]},{\"type\":\"never\"}]}}" );
  return node;
}
}

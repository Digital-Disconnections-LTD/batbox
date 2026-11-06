#include "settings/schema/Descriptor.hpp"
namespace batbox::settings::schema {
Json settingsDescriptor(){
  static const Json descriptor=value::parse(
    "{\"format\":1,\"base\":{\"type\":\"object\",\"fields\":[[\"$schema\",{\"type\":\"optional\",\"inner\":{\"type\":\"literal\",\"values\":[\"batbox://settings-sche"
    "ma\"]}}],[\"cleanupPeriodDays\",{\"type\":\"optional\",\"inner\":{\"type\":\"number\",\"checks\":[{\"kind\":\"greater_than\",\"value\":0,\"inclusive\":true},{\""
    "kind\":\"number_format\",\"format\":\"safeint\"}]}}],[\"env\",{\"type\":\"optional\",\"inner\":{\"type\":\"native\",\"name\":\"environmentVariables\"}}],[\"attr"
    "ibution\",{\"type\":\"optional\",\"inner\":{\"type\":\"object\",\"fields\":[[\"commit\",{\"type\":\"optional\",\"inner\":{\"type\":\"string\"}}],[\"pr\",{\"type"
    "\":\"optional\",\"inner\":{\"type\":\"string\"}}]],\"passthrough\":false}}],[\"includeCoAuthoredBy\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],["
    "\"includeGitInstructions\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"permissions\",{\"type\":\"optional\",\"inner\":{\"type\":\"native\",\"na"
    "me\":\"permissions\"}}],[\"model\",{\"type\":\"optional\",\"inner\":{\"type\":\"string\"}}],[\"enableAllProjectMcpServers\",{\"type\":\"optional\",\"inner\":{\""
    "type\":\"boolean\"}}],[\"disabledMcpjsonServers\",{\"type\":\"optional\",\"inner\":{\"type\":\"array\",\"element\":{\"type\":\"string\"}}}],[\"hooks\",{\"type\""
    ":\"optional\",\"inner\":{\"type\":\"import\",\"name\":\"hooks\"}}],[\"worktree\",{\"type\":\"optional\",\"inner\":{\"type\":\"object\",\"fields\":[[\"symlinkDir"
    "ectories\",{\"type\":\"optional\",\"inner\":{\"type\":\"array\",\"element\":{\"type\":\"string\"}}}],[\"sparsePaths\",{\"type\":\"optional\",\"inner\":{\"type\""
    ":\"array\",\"element\":{\"type\":\"string\"}}}]],\"passthrough\":false}}],[\"allowedHttpHookUrls\",{\"type\":\"optional\",\"inner\":{\"type\":\"array\",\"elemen"
    "t\":{\"type\":\"string\"}}}],[\"httpHookAllowedEnvVars\",{\"type\":\"optional\",\"inner\":{\"type\":\"array\",\"element\":{\"type\":\"string\"}}}],[\"outputStyl"
    "e\",{\"type\":\"optional\",\"inner\":{\"type\":\"string\"}}],[\"language\",{\"type\":\"optional\",\"inner\":{\"type\":\"string\"}}],[\"skipWebFetchPreflight\",{"
    "\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"spinnerTipsEnabled\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"syntaxHighlighting"
    "Disabled\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"alwaysThinkingEnabled\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"eff"
    "ortLevel\",{\"type\":\"catch\",\"inner\":{\"type\":\"optional\",\"inner\":{\"type\":\"enum\",\"values\":[\"low\",\"medium\",\"high\"]}}}],[\"advisorModel\",{\"t"
    "ype\":\"optional\",\"inner\":{\"type\":\"string\"}}],[\"fastMode\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"fastModePerSessionOptIn\",{\"typ"
    "e\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"promptSuggestionEnabled\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"showClearContextOn"
    "PlanAccept\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"agent\",{\"type\":\"optional\",\"inner\":{\"type\":\"string\"}}],[\"plansDirectory\",{"
    "\"type\":\"optional\",\"inner\":{\"type\":\"string\"}}],[\"autoMemoryEnabled\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"autoMemoryDirectory"
    "\",{\"type\":\"optional\",\"inner\":{\"type\":\"string\"}}],[\"autoDreamEnabled\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"skipDangerousModeP"
    "ermissionPrompt\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"cronEnabled\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"cronDurable\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"shellSnapshot\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"gadgetResultBudget\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}]],\"passthrough\":true}}" );
  return descriptor;
}
}

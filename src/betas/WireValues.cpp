#include "betas/WireValues.hpp"

namespace batbox::betas {
Headers makeProviderWireHeaders(bool connectorTextEnabled,
    bool internalUser){
  Headers value;
  value.core="claude-code-20250219";
  value.internal=internalUser?"cli-internal-2026-02-09":"";
  value.oauth="oauth-2025-04-20";
  value.context1m="context-1m-2025-08-07";
  value.interleavedThinking="interleaved-thinking-2025-05-14";
  value.redactThinking="redact-thinking-2026-02-12";
  value.summarizeConnectorText=connectorTextEnabled
    ?"summarize-connector-text-2026-03-13":"";
  value.contextManagement="context-management-2025-06-27";
  value.structuredOutputs="structured-outputs-2025-12-15";
  value.tokenEfficientGadgets="token-efficient-tools-2026-03-28";
  value.webSearch="web-search-2025-03-05";
  value.poetryStashingScope="prompt-caching-scope-2026-01-05";
  value.gadgetSearchFirstParty="advanced-tool-use-2025-11-20";
  value.gadgetSearchThirdParty="tool-search-tool-2025-10-19";
  value.bedrockExtraBodyParams={value.interleavedThinking,
    value.context1m,value.gadgetSearchThirdParty};
  return value;
}
}

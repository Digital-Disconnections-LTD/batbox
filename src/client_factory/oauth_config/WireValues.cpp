#include "client_factory/oauth_config/WireValues.hpp"

namespace batbox::client_factory::oauth_config {
Data makeArchivedProviderOAuthData(bool internalAtModuleLoad){
  Data data;
  data.production={
    "https://api.anthropic.com",
    "https://platform.claude.com/oauth/authorize",
    "https://claude.com/cai/oauth/authorize",
    "https://claude.ai",
    "https://platform.claude.com/v1/oauth/token",
    "https://api.anthropic.com/api/oauth/claude_cli/create_api_key",
    "https://api.anthropic.com/api/oauth/claude_cli/roles",
    "https://platform.claude.com/buy_credits?returnUrl=/oauth/code/"
      "success%3Fapp%3Dclaude-code",
    "https://platform.claude.com/oauth/code/success?app=claude-code",
    "https://platform.claude.com/oauth/code/callback",
    "9d1c250a-e61b-44d9-88ed-5944d1962f5e","",
    "https://mcp-proxy.anthropic.com","/v1/mcp/{server_id}"};
  if(internalAtModuleLoad)data.staging=Config{
    "https://api-staging.anthropic.com",
    "https://platform.staging.ant.dev/oauth/authorize",
    "https://claude-ai.staging.ant.dev/oauth/authorize",
    "https://claude-ai.staging.ant.dev",
    "https://platform.staging.ant.dev/v1/oauth/token",
    "https://api-staging.anthropic.com/api/oauth/claude_cli/"
      "create_api_key",
    "https://api-staging.anthropic.com/api/oauth/claude_cli/roles",
    "https://platform.staging.ant.dev/buy_credits?returnUrl=/oauth/"
      "code/success%3Fapp%3Dclaude-code",
    "https://platform.staging.ant.dev/oauth/code/success?app=claude-code",
    "https://platform.staging.ant.dev/oauth/code/callback",
    "22422756-60c9-4084-8eb7-27705fd5cf9a","-staging-oauth",
    "https://mcp-proxy-staging.anthropic.com","/v1/mcp/{server_id}"};
  data.local.defaultApiBase="http://localhost:8000";
  data.local.defaultAppBase="http://localhost:4000";
  data.local.defaultConsoleBase="http://localhost:3000";
  data.local.clientId="22422756-60c9-4084-8eb7-27705fd5cf9a";
  data.local.mcpProxyUrl="http://localhost:8205";
  data.local.mcpProxyPath="/v1/toolbox/shttp/mcp/{server_id}";
  data.local.routes={
    "/oauth/authorize","/oauth/authorize","/v1/oauth/token",
    "/api/oauth/claude_cli/create_api_key",
    "/api/oauth/claude_cli/roles",
    "/buy_credits?returnUrl=/oauth/code/success%3Fapp%3Dclaude-code",
    "/oauth/code/success?app=claude-code",
    "/oauth/code/callback"};
  data.customRoutes={
    "/oauth/authorize","/oauth/authorize","/v1/oauth/token",
    "/api/oauth/claude_cli/create_api_key",
    "/api/oauth/claude_cli/roles",
    "/oauth/code/success?app=claude-code",
    "/oauth/code/success?app=claude-code",
    "/oauth/code/callback"};
  data.allowedCustomBases={
    "https://beacon.claude-ai.staging.ant.dev",
    "https://claude.fedstart.com",
    "https://claude-staging.fedstart.com"};
  return data;
}
}

#define main batsResumeProbeMain
#include "bats_resume_probe.cpp"
#undef main
#include <stdexcept>

int main(){
  const Json bat={{"agentType","worker"},{"model","worker-model"},{"builtIn",false}};
  const Json item={{"agentId","agent-typed"},{"prompt","continue"},
    {"nativeState",true},
    {"transcript",{{"messages",Json::array({Json{{"type","user"},
      {"message",{{"content","old"}}}}})},{"contentReplacements",Json::array()}}},
    {"meta",{{"agentType","worker"},{"description","worker"}}},
    {"activeAgents",Json::array({bat})},
    {"appState",{{"toolPermissionContext",{{"mode","default"},
      {"additionalWorkingDirectories",Json::array()}}},
      {"mcp",{{"tools",Json::array()}}},
      {"agentDefinitions",{{"activeAgents",Json::array({bat})}}}}}};
  const Json output=run(item);
  const Json& launched=output.at("launched");
  if(!output.at("error").is_null() || launched.at("nativeReplacementIdentity")!=true ||
      launched.at("nativeReplacementParentSeen")!=true ||
      launched.at("nativeReplacementResumedSeen")!=true ||
      !launched.at("contentReplacementState").is_null())
    throw std::runtime_error("Typed resume reconstruction identity was lost: "+
      output.dump());
  std::cout<<"typed resume replacement state forwarded by identity\n";
}

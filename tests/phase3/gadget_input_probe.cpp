#include "gadgets/InputNormalization.hpp"
#include <iostream>
#include <string>

using batbox::Json;
namespace gadgets=batbox::gadgets;
static Json evaluate(const Json& test){
  if(test.at("op")=="strip")return Json{{"result",gadgets::stripTrailingWhitespace(test.at("text").get<std::string>())}};
  int snapshotCalls=0;
  gadgets::GadgetInputServices s;
  s.exitPlanModeGadgetName="ExitPlanMode";s.missionOutputGadgetName="TaskOutput";
  s.bashGadgetName="Bash";s.fileEditGadgetName="Edit";s.fileWriteGadgetName="Write";
  s.getPlan=[&](const std::optional<std::string>&)->std::optional<std::string>{
    if(test.contains("plan")&&!test.at("plan").is_null())return test.at("plan").get<std::string>();
    return std::nullopt;
  };
  s.getPlanFilePath=[&](const std::optional<std::string>&){return test.value("planFilePath",std::string("/tmp/plan.md"));};
  s.persistFileSnapshotIfRemote=[&](){++snapshotCalls;};
  s.parseBashInput=[](const Json& input){return input;};
  s.parseFileEditInput=[](const Json& input){return input;};
  s.parseFileWriteInput=[](const Json& input){return input;};
  s.getCwd=[&](){return test.value("cwd",std::string("/repo"));};
  s.getPlatform=[&](){return test.value("platform",std::string("linux"));};
  s.windowsPathToPosixPath=[](const std::string& path){std::string result=path;
    for(char& c:result)if(c=='\\')c='/';
    return result;};
  s.normalizeFileEditInput=[](const Json& value){Json result=value;
    for(auto& edit:result["edits"]){
      if(edit.contains("old_string")&&edit["old_string"].is_string()){
        std::string text=edit["old_string"].get<std::string>();std::size_t pos=0;
        while((pos=text.find("CURVE",pos))!=std::string::npos){text.replace(pos,5,"straight");pos+=8;}
        edit["old_string"]=text;
      }
    }
    return result;};
  gadgets::Gadget gadget;gadget.name=test.at("tool").get<std::string>();
  const Json& input=test.at("input");
  const std::optional<std::string> batId=test.contains("agentId")&&test.at("agentId").is_string()
    ?std::optional<std::string>(test.at("agentId").get<std::string>()):std::nullopt;
  const Json result=test.at("op")=="normalize"
    ?gadgets::normalizeGadgetInput(gadget,input,batId,s)
    :gadgets::normalizeGadgetInputForAPI(gadget,input,s);
  return Json{{"result",result},{"snapshotCalls",snapshotCalls}};
}
int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<batbox::value::stringify(evaluate(batbox::value::parse(line)))<<'\n';}
  catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}
}}

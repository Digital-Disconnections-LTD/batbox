#include "instructions/hooks/runtime/ProcessHookJson.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  hook::ProcessHookJsonInput input;
  input.json=item.at("json");input.command="command-1";
  input.hookName="PreToolUse:Bash";input.gadgetUseID="tool-1";
  input.hookEvent="PreToolUse";
  if(item.contains("expectedHookEvent"))input.expectedHookEvent=
    item.at("expectedHookEvent").get<std::string>();
  if(item.contains("stdout"))input.stdoutText=item.at("stdout").get<std::string>();
  if(item.contains("stderr"))input.stderrText=item.at("stderr").get<std::string>();
  if(item.contains("exitCode"))input.exitCode=item.at("exitCode").get<double>();
  if(item.contains("durationMs"))input.durationMs=item.at("durationMs").get<double>();
  hook::ProcessHookJsonServices services;
  services.createAttachmentMessage=[](const Json& attachment,
    const std::vector<std::string>& undefined){
    return Json{{"attachment",attachment},{"ownUndefined",undefined}};
  };
  Json value=nullptr,error=nullptr;
  try{
    auto result=hook::processHookJSONOutput(input,services);
    std::sort(result.ownUndefinedKeys.begin(),result.ownUndefinedKeys.end());
    value={{"data",result.fields},{"ownUndefined",result.ownUndefinedKeys}};
  }catch(const std::exception& failure){error=failure.what();}
  return {{"name",item.at("name")},{"value",value},{"error",error}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<rows.dump()<<'\n';
}

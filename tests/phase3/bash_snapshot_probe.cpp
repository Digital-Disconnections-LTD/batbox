#include "shell/SnapshotContent.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using batbox::Json;
int main(){std::string line;while(std::getline(std::cin,line))try{
  const auto input=batbox::value::parse(line);
  batbox::shell_snapshot::ContentServices services;
  services.ripgrepCommand=[input]{
    batbox::shell_snapshot::RipgrepCommand result;
    result.path=input.value("rgPath",std::string("/usr/bin/rg"));
    result.args=input.value("rgArgs",std::vector<std::string>());
    if(input.contains("argv0")&&input["argv0"].is_string())
      result.argv0=input["argv0"].get<std::string>();
    return result;
  };
  services.hasEmbeddedSearchGadgets=[input]{return input.value("embedded",false);};
  services.embeddedSearchGadgetsBinaryPath=[input]{return input.value("embeddedPath",std::string("/opt/embedded"));};
  services.pathValue=[input]{return input.value("pathValue",std::string("/usr/bin"));};
  services.cygwinPathValue=[input]()->std::optional<std::string>{
    return input.value("cygwinPath",std::string("/usr/bin"));};
  services.windows=input.value("windows",false);
  const auto op=input.at("op").get<std::string>();
  const auto home=input.value("home",std::string("/home/user"));
  const auto shellPath=input.value("shellPath",std::string("/bin/bash"));
  Json result;
  if(op=="argv0")result=batbox::shell_snapshot::createArgv0ShellFunction(
    input.value("funcName",std::string("rg")),input.value("argv0",std::string("rg")),
    input.value("binary",std::string("/usr/bin/rg")),
    input.value("prependArgs",std::vector<std::string>()));
  else if(op=="rg"){
    const auto value=batbox::shell_snapshot::createRipgrepShellIntegration(services);
    result={{"type",value.type},{"snippet",value.snippet}};
  }else if(op=="findgrep"){
    const auto value=batbox::shell_snapshot::createFindGrepShellIntegration(services);
    result=value?Json(*value):Json();
  }else if(op=="config")result=batbox::shell_snapshot::getConfigFile(shellPath,home);
  else if(op=="user")result=batbox::shell_snapshot::getUserSnapshotContent(
    input.value("configFile",std::string("/home/user/.bashrc")));
  else if(op=="product")result=batbox::shell_snapshot::getBatboxSnapshotContent(services);
  else if(op=="script")result=batbox::shell_snapshot::getSnapshotScript(shellPath,
    input.value("snapshotPath",std::string("/tmp/snapshot.sh")),
    input.value("configExists",false),home,services);
  else throw std::logic_error("unknown op");
  std::cout<<batbox::value::stringify(Json{{"result",result}})<<'\n';
}catch(const std::exception& error){
  std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';
}}

#include "gadgets/bash/CwdPolicy.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);const std::string data(std::istreambuf_iterator<char>{file},{});
  const auto cases=batbox::value::parse(data);batbox::Json output=batbox::Json::array();
  for(const auto& item:cases){
    batbox::Json trace=batbox::Json::array();
    batbox::bash_gadget::CwdPolicyServices services;
    services.getCwd=[&]{trace.push_back("getCwd");return item.at("cwd").get<std::string>();};
    services.getOriginalCwd=[&]{trace.push_back("getOriginalCwd");return item.at("original").get<std::string>();};
    services.shouldMaintainProjectWorkingDir=[&]{trace.push_back("shouldMaintain");return item.at("maintain").get<bool>();};
    services.pathInAllowedWorkingPath=[&](const std::string& cwd,const batbox::Json& context){
      trace.push_back(batbox::Json::array({"allowed",cwd,context}));return item.at("allowed").get<bool>();};
    services.setCwd=[&](const std::string& cwd){trace.push_back(batbox::Json::array({"setCwd",cwd}));};
    const bool reset=batbox::bash_gadget::resetCwdIfOutsideProject(
      batbox::Json{{"allowedPaths",batbox::Json::array({"/project"})}},services);
    output.push_back({{"reset",reset},{"trace",trace},
      {"message",batbox::bash_gadget::stdErrAppendShellResetMessage(
        item.at("stderr").get<std::string>(),item.at("original").get<std::string>())}});
  }
  std::cout<<batbox::value::stringify(output)<<'\n';
}

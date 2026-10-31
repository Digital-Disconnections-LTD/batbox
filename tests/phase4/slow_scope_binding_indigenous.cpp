#include "diagnostics/SlowScopeBinding.hpp"
#include "diagnostics/ExecSyncWrapper.hpp"
#include "fs_operations/NodeFsSync.hpp"
#include "runtime/JsonValue.hpp"
#include <cassert>
#include <memory>
#include <string>
#include <vector>

int main(){
  double current=0;
  bool reentered=false;
  std::vector<std::string> logs,added;
  batbox::fs_operations::NodeFsSync* active=nullptr;
  batbox::diagnostics::SlowOperationConfig config;
  config.featureEnabled=true;config.thresholdEnvironment="0";
  batbox::diagnostics::SlowOperationImports imports;
  imports.performanceNow=[&]{current+=10;return current;};
  imports.captureStack=[](){return std::optional<std::string>(
    "Error\n at /app/native.ts:8:3");};
  imports.parseJson=[](const std::string& text){return batbox::value::parse(text);};
  imports.logForDebugging=[&](const std::string& line){
    logs.push_back(line);
    if(!reentered){reentered=true;assert(active->existsSync("/nested"));}
  };
  imports.addSlowOperation=[&](const std::string& name,double){added.push_back(name);};
  auto operations=std::make_shared<batbox::diagnostics::SlowOperations>(
    std::move(config),std::move(imports));
  const auto scope=batbox::diagnostics::bindSlowLogging(operations);
  batbox::fs_operations::NodeFsSyncImports fileImports;
  fileImports.beginSlowLogging=scope;
  fileImports.existsSync=[](const std::string&){return true;};
  batbox::fs_operations::NodeFsSync file(std::move(fileImports));
  active=&file;
  assert(file.existsSync("/outer"));
  assert(logs.size()==1&&added.size()==1);
  assert(logs.front().find("fs.existsSync(/outer)")!=std::string::npos);

  batbox::diagnostics::ExecSyncImports exec;
  exec.beginSlowLogging=scope;
  exec.nodeExecSync=[](const std::string&,const std::optional<batbox::Json>&){
    return batbox::diagnostics::ExecSyncResult(std::string("ok"));};
  const auto answer=batbox::diagnostics::execSyncDeprecated("echo ok",{},exec);
  assert(std::get<std::string>(answer)=="ok");
  assert(logs.size()==2&&added.size()==2);
  assert(logs.back().find("execSync: echo ok")!=std::string::npos);
}

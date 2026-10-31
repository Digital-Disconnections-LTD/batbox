#include "instructions/project/async/ExternalWarning.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>

using namespace batbox;
namespace project=batbox::instructions::project;
namespace async=batbox::instructions::project::async;
int main(int argc,char** argv){
  if(argc!=2)return 2;std::ifstream stream(argv[1]);
  const std::string data(std::istreambuf_iterator<char>{stream},{});
  Json rows=Json::array();
  for(const auto& item:value::parse(data)){
    event_loop::Loop loop;Json logs=Json::array();
    async::ExternalWarningImports imports;
    imports.getCurrentProjectConfig=[&]{logs.push_back("config");
      return async::ExternalIncludeConfig{item.at("approved").get<bool>(),
        item.at("warningShown").get<bool>()};};
    imports.getForcedMemoryFiles=[&]{logs.push_back("load:true");
      async::Deferred<async::MemoryFiles> pending(loop);
      if(item.value("reject",false))pending.reject(
        std::make_exception_ptr(std::runtime_error("load failed")));
      else{
        async::MemoryFiles files;
        for(const auto& entry:item.at("files")){
          project::MemoryFileInfo file;file.path=entry.at("path").get<std::string>();
          file.type=entry.at("type").get<std::string>();
          if(entry.contains("parent"))file.parent=entry.at("parent").get<std::string>();
          files.push_back(std::move(file));
        }
        pending.resolve(std::move(files));
      }
      return pending;};
    project::DiscoveryServices sync;
    sync.originalCwd=[] {return std::string("/project");};
    sync.pathInWorkingPath=[&](const std::string& file,const std::string&){
      logs.push_back("inside:"+file);return file.rfind("/project/",0)==0;};
    imports.hasExternalIncludes=[&](const async::MemoryFiles& files){
      return project::hasExternalBatboxMdIncludes(files,sync);};
    auto pending=async::shouldShowExternalIncludesWarning(loop,imports);
    std::optional<bool> result;std::optional<std::string> error;
    pending.then([&](const bool* value,const std::exception_ptr& failure){
      if(failure){try{std::rethrow_exception(failure);}catch(const std::exception& e){
        error=e.what();}}
      else result=*value;});
    loop.runReady();
    if(error)rows.push_back({{"name",item.at("name")},{"error",*error},{"logs",logs}});
    else if(result)rows.push_back({{"name",item.at("name")},
      {"value",*result},{"logs",logs}});
    else throw std::runtime_error("External warning pending");
  }
  std::cout<<value::stringify(rows)<<'\n';
}

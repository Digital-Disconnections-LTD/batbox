#include "git/GitState.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>

using namespace batbox;
int main(int argc,char** argv){
  if(argc!=2)return 2;std::ifstream stream(argv[1]);
  const std::string data(std::istreambuf_iterator<char>{stream},{});
  Json rows=Json::array();
  for(const auto& item:value::parse(data)){
    event_loop::Loop loop;Json calls=Json::array(),order=Json::array();
    auto should=[&](const std::string& kind,const std::string& name){
      return !item.at(kind).is_null()&&item.at(kind).get<std::string>()==name;};
    auto string=[&](const std::string& name,const std::string& answer){
      return [&,name,answer]{calls.push_back(name);
        if(should("throw",name))throw std::runtime_error(name+" failed");
        event_loop::Deferred<std::string> value(loop);
        if(should("fail",name))value.reject(std::make_exception_ptr(std::runtime_error(name+" failed")));
        else value.resolve(answer);return value;};};
    git::GitStateImports imports;
    imports.getHead=string("head","abc");imports.getBranch=string("branch","main");
    imports.getRemoteUrl=[&]{calls.push_back("remote");
      event_loop::Deferred<std::optional<std::string>> value(loop);
      if(should("fail","remote"))value.reject(std::make_exception_ptr(std::runtime_error("remote failed")));
      else value.resolve(std::nullopt);return value;};
    auto boolean=[&](const std::string& name,bool answer){return [&,name,answer]{
      calls.push_back(name);event_loop::Deferred<bool> value(loop);value.resolve(answer);return value;};};
    imports.getIsHeadOnRemote=boolean("onRemote",true);
    imports.getIsClean=boolean("clean",false);
    imports.getWorktreeCount=[&]{calls.push_back("worktrees");
      event_loop::Deferred<std::size_t> value(loop);value.resolve(2);return value;};
    auto future=git::getGitState(loop,imports);
    std::optional<std::optional<git::GitRepoState>> result;
    future.then([&](const std::optional<git::GitRepoState>* value,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);result=*value;order.push_back("done");});
    loop.queueMicrotask([&]{order.push_back("sibling");});loop.runReady();
    if(!result)throw std::runtime_error("Git state pending");
    Json value=nullptr;
    if(*result){const auto& state=**result;
      value={{"commitHash",state.commitHash},{"branchName",state.branchName},
        {"remoteUrl",state.remoteUrl?Json(*state.remoteUrl):Json(nullptr)},
        {"isHeadOnRemote",state.isHeadOnRemote},{"isClean",state.isClean},
        {"worktreeCount",state.worktreeCount}};}
    rows.push_back({{"name",item.at("name")},{"value",value},
      {"calls",calls},{"order",order}});
  }
  std::cout<<value::stringify(rows)<<'\n';
}

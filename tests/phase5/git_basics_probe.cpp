#include "git/GitBasics.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace batbox;
namespace {
std::string message(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}catch(const std::exception& value){return value.what();}
}
template<class T,class Convert>Json observe(event_loop::Loop& loop,git::Deferred<T> pending,
  Convert convert){
  Json result;bool finished=false;
  pending.then([&](const T* value,const std::exception_ptr& error){
    if(error)result={{"error",message(error)}};
    else result={{"value",convert(*value)}};
    finished=true;
  });
  loop.runReady();if(!finished)throw std::runtime_error("Git fixture promise pending");
  return result;
}
Json evaluate(const Json& item){
  event_loop::Loop loop;Json logs=Json::array(),records=Json::array();
  git::Deferred<std::optional<std::string>> gitDir(loop);
  gitDir.resolve(std::optional<std::string>("/repo/.git"));
  git::Deferred<bool> shallowPromise(loop);shallowPromise.resolve(false);
  git::BasicsImports imports;
  std::size_t dateCalls=0;
  imports.nowMillis=[&]{logs.push_back("now");
    return static_cast<std::int64_t>(dateCalls++==0?
      item.value("startMs",1000):item.value("endMs",1017));};
  imports.logDiagnostic=[&](const std::string& level,
    const std::string& event,const std::optional<Json>& payload){
    Json row={{"diag",event},{"level",level}};
    if(payload)row["payload"]=*payload;
    logs.push_back(std::move(row));
    if(event=="is_git_check_started"&&item.value("diagStartThrow",false))
      throw std::runtime_error("start diagnostic failed");
    if(event=="is_git_check_completed"&&item.value("diagCompleteThrow",false))
      throw std::runtime_error("complete diagnostic failed");
  };
  imports.getCwd=[&]{logs.push_back("getCwd");
    if(item.value("cwdError",false))throw std::runtime_error("cwd failed");
    return item.at("cwd").get<std::string>();};
  imports.findGitRoot=[&](const std::string& cwd)->std::optional<std::string>{
    logs.push_back("findRoot:"+cwd);
    if(cwd==item.at("cwd").get<std::string>()&&!item.at("root").is_null())
      return item.at("root").get<std::string>();
    return std::nullopt;
  };
  imports.resolveGitDir=[&](const std::string& cwd){
    logs.push_back("resolveGitDir:"+cwd);return gitDir;};
  imports.realpath=[&](const std::string& path){
    logs.push_back("realpath:"+path);git::Deferred<std::string> output(loop);
    if(item.value("realpathThrow",false))throw std::runtime_error("realpath failed");
    if(item.value("realpathError",false))output.reject(
      std::make_exception_ptr(std::runtime_error("realpath failed")));
    else output.resolve((path==item.at("cwd").get<std::string>()?
      item.at("resolvedCwd"):item.at("resolvedRoot")).get<std::string>());
    return output;
  };
  imports.whichSync=[&](const std::string& name)->std::optional<std::string>{
    logs.push_back("which:"+name);
    if(item.at("which").is_null())return std::nullopt;
    return item.at("which").get<std::string>();
  };
  auto stringGetter=[&](const std::string& name,const std::string& value){
    return [&,name,value]{logs.push_back(name);git::Deferred<std::string> output(loop);
      output.resolve(value);return output;};
  };
  imports.getStashedHead=stringGetter("head","abc");
  imports.getStashedBranch=stringGetter("branch","feature");
  imports.getStashedDefaultBranch=stringGetter("default","main");
  imports.getStashedRemoteUrl=[&]{logs.push_back("remote");
    git::Deferred<std::optional<std::string>> output(loop);
    output.resolve("https://example/repo");return output;};
  imports.getWorktreeCountFromFs=[&]{logs.push_back("worktrees");
    git::Deferred<std::size_t> output(loop);output.resolve(2);return output;};
  imports.isShallowCloneFs=[&]{logs.push_back("shallow");return shallowPromise;};
  git::GitBasics git(loop,imports);
  const auto exe=git.gitExe();const bool same=git.gitExe()==git.gitExe();
  records.push_back({{"op","gitExe"},{"value",exe},{"same",same}});
  const auto first=git.getIsGit(),second=git.getIsGit();
  auto isGit=observe(loop,first,[](bool value)->Json{return value;});
  isGit["op"]="getIsGit";isGit["same"]=first.samePromise(second);records.push_back(isGit);
  const auto dir=git.getGitDir(item.at("cwd").get<std::string>());
  auto dirRecord=observe(loop,dir,[](const std::optional<std::string>& value)->Json{
    return value?Json(*value):Json(nullptr);});
  dirRecord["op"]="getGitDir";dirRecord["same"]=dir.samePromise(gitDir);records.push_back(dirRecord);
  auto at=observe(loop,git.isAtGitRoot(),[](bool value)->Json{return value;});
  at["op"]="isAtGitRoot";records.push_back(at);
  auto in=observe(loop,git.dirIsInGitRepo("/outside"),[](bool value)->Json{return value;});
  in["op"]="dirIsInGitRepo";records.push_back(in);
  auto head=observe(loop,git.getHead(),[](const std::string& value)->Json{return value;});
  head["op"]="getHead";records.push_back(head);
  Json order=Json::array();auto branch=git.getBranch();std::optional<std::string> branchValue;
  branch.then([&](const std::string* value,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);branchValue=*value;order.push_back("branch-done");});
  loop.queueMicrotask([&]{order.push_back("sibling");});loop.runReady();
  if(!branchValue)throw std::runtime_error("branch not settled");
  records.push_back({{"op","getBranch"},{"value",*branchValue},{"order",order}});
  auto def=observe(loop,git.getDefaultBranch(),[](const std::string& value)->Json{return value;});
  def["op"]="getDefaultBranch";records.push_back(def);
  auto remote=observe(loop,git.getRemoteUrl(),[](const std::optional<std::string>& value)->Json{
    return value?Json(*value):Json(nullptr);});
  remote["op"]="getRemoteUrl";records.push_back(remote);
  auto count=observe(loop,git.getWorktreeCount(),[](std::size_t value)->Json{return value;});
  count["op"]="getWorktreeCount";records.push_back(count);
  const auto shallow=git.isShallowClone();
  auto shallowRecord=observe(loop,shallow,[](bool value)->Json{return value;});
  shallowRecord["op"]="isShallowClone";
  shallowRecord["same"]=shallow.samePromise(shallowPromise);records.push_back(shallowRecord);
  return Json{{"name",item.at("name")},{"records",records},{"logs",logs}};
}
}
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream stream(argv[1]);const std::string input(std::istreambuf_iterator<char>{stream},{});
  Json out=Json::array();for(const auto& item:value::parse(input))out.push_back(evaluate(item));
  std::cout<<value::stringify(out)<<'\n';
}

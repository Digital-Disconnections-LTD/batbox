#include "git_filesystem/GitFilesystem.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace batbox;
using namespace batbox::git_filesystem;
namespace {
template<class T>Json await(event_loop::Loop& loop,Deferred<T> pending,
  std::function<Json(const T&)> convert){
  Json result;
  bool finished=false;
  pending.then([&](const T* value,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    result=convert(*value);finished=true;
  });
  loop.run();if(!finished)throw std::runtime_error("Git fixture promise did not settle");
  return result;
}
template<class T>Json optional(const std::optional<T>& value){
  return value?Json(*value):Json();
}
Json evaluate(const Json& item){
  event_loop::Loop loop;
  auto files=item.value("files",Json::object());
  const auto dirs=item.value("dirs",Json::array());
  const auto entries=item.value("entries",Json::object());
  Json logs=Json::array(),records=Json::array();
  std::map<std::string,std::function<void()>> watches;
  std::optional<std::string> delayedPath;
  std::optional<Deferred<std::string>> delayedRead;
  std::optional<std::string> delayedCaptured;
  std::optional<Deferred<std::string>> pendingHead;
  Services services;
  services.readFile=[&](const std::string& path){
    logs.push_back("read:"+path);
    Deferred<std::string> output(loop);
    if(delayedPath&&*delayedPath==path){
      delayedPath.reset();delayedRead=output;
      delayedCaptured=files.at(path).get<std::string>();
    }else if(files.contains(path))output.resolve(files[path].get<std::string>());
    else output.reject(std::make_exception_ptr(std::runtime_error("ENOENT")));
    return output;
  };
  services.stat=[&](const std::string& path){
    logs.push_back("stat:"+path);Deferred<FileStat> output(loop);
    bool dir=false;for(const auto& value:dirs)if(value==path){dir=true;break;}
    if(files.contains(path))output.resolve(FileStat{true,false});
    else if(dir)output.resolve(FileStat{false,true});
    else output.reject(std::make_exception_ptr(std::runtime_error("ENOENT")));
    return output;
  };
  services.readdir=[&](const std::string& path){
    logs.push_back("readdir:"+path);Deferred<std::vector<std::string>> output(loop);
    if(entries.contains(path))output.resolve(entries[path].get<std::vector<std::string>>());
    else output.reject(std::make_exception_ptr(std::runtime_error("ENOENT")));
    return output;
  };
  services.findGitRoot=[&](const std::string& cwd)->std::optional<std::string>{
    logs.push_back("findGitRoot:"+cwd);
    return item.contains("root")&&!item["root"].is_null()?
      std::optional(item["root"].get<std::string>()):std::nullopt;
  };
  services.getCwd=[&]{logs.push_back("getCwd");return item.at("cwd").get<std::string>();};
  services.parseGitConfigValue=[&](const std::string& dir,const std::string&,
    const std::optional<std::string>&,const std::string&){
    logs.push_back("parseConfig:"+dir);
    Deferred<std::optional<std::string>> output(loop);
    const auto urls=item.value("urls",Json::object());
    output.resolve(urls.contains(dir)?std::optional(urls[dir].get<std::string>()):std::nullopt);
    return output;
  };
  services.waitForScrollIdle=[&]{logs.push_back("scrollIdle");
    Deferred<Unit> output(loop);output.resolve(Unit{});return output;};
  services.watchFile=[&](const std::string& path,std::uint64_t,
    std::function<void()> callback){logs.push_back("watch:"+path);watches[path]=std::move(callback);};
  services.unwatchFile=[&](const std::string& path){logs.push_back("unwatch:"+path);watches.erase(path);};
  services.registerCleanup=[&](std::function<void()>){logs.push_back("registerCleanup");};
  services.watchIntervalMs=10;
  GitFilesystem git(loop,services);
  for(const auto& step:item.at("steps")){
    const auto op=step.at("op").get<std::string>();
    if(op=="write"){
      files[step.at("path").get<std::string>()]=step.at("value");
      records.push_back({{"op","write"}});continue;
    }
    if(op=="fire"){
      const auto path=step.at("path").get<std::string>();
      const auto found=watches.find(path);
      if(found!=watches.end())found->second();
      loop.runReady();
      records.push_back({{"op","fire"},{"watched",watches.count(path)!=0}});continue;
    }
    if(op=="delayRead"){delayedPath=step.at("path").get<std::string>();
      records.push_back({{"op","delayRead"}});continue;}
    if(op=="beginGetCachedHead"){pendingHead=git.getStashedHead();loop.runReady();
      records.push_back({{"op","beginGetCachedHead"}});continue;}
    if(op=="releaseRead"){
      if(!delayedRead)throw std::runtime_error("delayed Git read not scheduled");
      delayedRead->resolve(*delayedCaptured);
      loop.runReady();records.push_back({{"op","releaseRead"}});continue;
    }
    if(op=="awaitPendingHead"){
      if(!pendingHead)throw std::runtime_error("Git head not pending");
      records.push_back({{"op","awaitPendingHead"},{"value",await<std::string>(loop,*pendingHead,
        [](const std::string& value)->Json{return value;})}});continue;
    }
    Json value;
    const auto path=step.value("path",std::string());
    if(op=="resolveGitDir")value=await<std::optional<std::string>>(loop,git.resolveGitDir(),optional<std::string>);
    else if(op=="readGitHead")value=await<std::optional<Head>>(loop,git.readGitHead(path),
      [](const std::optional<Head>& head)->Json{
        if(!head)return nullptr;
        return head->type=="branch"?Json{{"type","branch"},{"name",head->value}}:
          Json{{"type","detached"},{"sha",head->value}};
      });
    else if(op=="resolveRef")value=await<std::optional<std::string>>(loop,
      git.resolveRef(path,step.at("ref")),optional<std::string>);
    else if(op=="getCommonDir")value=await<std::optional<std::string>>(loop,
      git.getCommonDir(path),optional<std::string>);
    else if(op=="readRawSymref")value=await<std::optional<std::string>>(loop,
      git.readRawSymref(path,step.at("ref"),step.at("prefix")),optional<std::string>);
    else if(op=="getCachedBranch")value=await<std::string>(loop,git.getStashedBranch(),
      [](const std::string& value)->Json{return value;});
    else if(op=="getCachedHead")value=await<std::string>(loop,git.getStashedHead(),
      [](const std::string& value)->Json{return value;});
    else if(op=="getCachedRemoteUrl")value=await<std::optional<std::string>>(loop,
      git.getStashedRemoteUrl(),optional<std::string>);
    else if(op=="getCachedDefaultBranch")value=await<std::string>(loop,
      git.getStashedDefaultBranch(),[](const std::string& value)->Json{return value;});
    else if(op=="getHeadForDir")value=await<std::optional<std::string>>(loop,
      git.getHeadForDir(path),optional<std::string>);
    else if(op=="readWorktreeHeadSha")value=await<std::optional<std::string>>(loop,
      git.readWorktreeHeadSha(path),optional<std::string>);
    else if(op=="getRemoteUrlForDir")value=await<std::optional<std::string>>(loop,
      git.getRemoteUrlForDir(path),optional<std::string>);
    else if(op=="isShallowClone")value=await<bool>(loop,git.isShallowClone(),
      [](bool value)->Json{return value;});
    else if(op=="getWorktreeCountFromFs")value=await<std::size_t>(loop,
      git.getWorktreeCountFromFs(),[](std::size_t value)->Json{return value;});
    else throw std::runtime_error("unknown Git fixture op "+op);
    records.push_back({{"op",op},{"value",value}});
  }
  git.resetGitFileWatcher();
  return {{"name",item.at("name")},{"records",records},{"logs",logs}};
}
}
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);const auto cases=value::parse(
    std::string(std::istreambuf_iterator<char>{file},{}));
  Json result=Json::array();for(const auto& item:cases)result.push_back(evaluate(item));
  std::cout<<value::stringify(result)<<'\n';
}

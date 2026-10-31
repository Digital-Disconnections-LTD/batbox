#include "attachment_triggers/Attachments.hpp"
#include "runtime/AsyncLaunch.hpp"
#include <algorithm>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace batbox::attachment_triggers {
namespace {
namespace fs=std::filesystem;
struct DirResult {
  std::string dir;
  std::vector<std::string> names;
};
std::vector<std::string> scan(const std::string& dir){
  try{
    std::vector<std::string> candidates;
    for(const auto& entry:fs::directory_iterator(dir)){
      const auto kind=entry.symlink_status();
      if(fs::is_directory(kind)||fs::is_symlink(kind))
        candidates.push_back(entry.path().filename().string());
    }
    std::sort(candidates.begin(),candidates.end());
    std::vector<std::string> names;
    for(const auto& name:candidates){
      try{
        const auto path=fs::path(dir)/name/"SKILL.md";
        if(fs::exists(path))names.push_back(name);
      }catch(...){}
    }
    return names;
  }catch(...){return {};}
}
std::string displayPath(const std::string& dir,const std::string& cwd){
  const auto relative=fs::path(dir).lexically_relative(cwd);
  return relative.empty()?dir:relative.generic_string();
}
struct Run {
  event_loop::Deferred<Json> output;
  std::vector<DirResult> results;
  std::size_t remaining;
  std::shared_ptr<runtime::OrderedStringSet> triggers;
  std::string cwd;
  Run(event_loop::Loop& loop,std::size_t count,
    std::shared_ptr<runtime::OrderedStringSet> paths,std::string workingDir):
    output(loop),results(count),remaining(count),triggers(std::move(paths)),
    cwd(std::move(workingDir)){}
  void complete(std::size_t index,std::string dir,std::vector<std::string> names){
    results[index]={std::move(dir),std::move(names)};
    if(--remaining!=0)return;
    Json attachments=Json::array();
    for(const auto& result:results){
      if(result.names.empty())continue;
      attachments.push_back(Json{{"type","dynamic_skill"},
        {"skillDir",result.dir},{"skillNames",result.names},
        {"displayPath",displayPath(result.dir,cwd)}});
    }
    triggers->clear();
    output.resolve(std::move(attachments));
  }
};
}
event_loop::Deferred<Json> getDynamicSkillAttachments(execution::Context& context,
  const Services& services){
  if(!services.loop.isOwnerThread())
    throw std::logic_error("Dynamic skills require owner event loop");
  event_loop::Deferred<Json> empty(services.loop);
  auto triggers=context.dynamicSkillDirTriggers;
  if(!triggers||triggers->empty()){
    empty.resolve(Json::array());return empty;
  }
  std::vector<std::string> dirs(triggers->begin(),triggers->end());
  auto run=std::make_shared<Run>(services.loop,dirs.size(),triggers,services.cwd);
  for(std::size_t i=0;i<dirs.size();++i){
    const auto dir=dirs[i];
    try{
      runtime::contextThread([run,i,dir]{
        auto names=scan(dir);
        try{run->output.dispatcher().postMission([run,i,dir,
          names=std::move(names)]()mutable{
          run->complete(i,dir,std::move(names));
        });}catch(...){}
      }).detach();
    }catch(...){run->complete(i,dir,{});}
  }
  return run->output;
}
} // namespace batbox::attachment_triggers

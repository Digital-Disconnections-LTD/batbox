#include "application_flights/Restore.hpp"
#include "process/ExecFileNoThrow.hpp"
#include "runtime/IndigenousPath.hpp"
#include "flights/FlightListing.hpp"
#include "flights/Views.hpp"
#include "runtime/Platform.hpp"
#include "runtime/IndigenousUnicode.hpp"
#include "shell/ShellQuote.hpp"
#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <iostream>
#include <set>
#include <stdexcept>

namespace batbox::application_flights {
namespace {
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& owner,T value){
  event_loop::Deferred<T> output(owner);output.resolve(std::move(value));return output;
}
std::string string(const Json& value,const char* key){
  const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():"";
}
bool uuid(const std::string& id){
  if(id.size()!=36)return false;
  for(std::size_t i=0;i<id.size();++i){
    if(i==8||i==13||i==18||i==23){if(id[i]!='-')return false;}
    else if(!std::isxdigit(static_cast<unsigned char>(id[i])))return false;
  }
  return true;
}
std::string homeRelative(const std::string& path){
#ifdef _WIN32
  auto home=runtime::environmentUtf8("USERPROFILE");
  if(home.empty())home=runtime::environmentUtf8("HOME");
#else
  const char* value=std::getenv("HOME");
  std::string home=value?value:"";
#endif
  while(home.size()>1&&(home.back()=='/'||home.back()=='\\'))home.pop_back();
  if(home.empty()||home=="/")return path;
  if(path==home)return "~";
  if(path.size()>home.size()&&path.starts_with(home)&&
      (path[home.size()]=='/'||path[home.size()]=='\\'))
    return "~"+path.substr(home.size());
  return path;
}
std::string folded(std::string value){
  value=value::trim(value);
  std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){
    return static_cast<char>(std::tolower(c));});
  return value;
}
std::string commandFor(const Json& log,const std::string& id){
  const auto project=string(log,"projectPath");
  if(project.empty())return "batbox --resume "+id;
  return "cd "+shell_quote::quote({Json(project)})+" && batbox --resume "+id;
}
} // namespace

event_loop::Deferred<std::vector<std::string>> Restore::worktreePaths(
    const std::string& cwd){
  event_loop::Deferred<std::vector<std::string>> output(inputs_.owner);
  process::ExecImports imports;
  imports.execa=process::indigenousExeca(inputs_.owner);
  imports.getCwd=[cwd]{return cwd;};
  imports.logError=inputs_.logError;
  process::ExecOptions options;options.cwd=cwd;options.timeout=10000;
  options.preserveOutputOnError=false;
  process::execFileNoThrowWithCwd(inputs_.owner,"git",
    {"worktree","list","--porcelain"},options,imports).then(
    [output,cwd](const process::ExecResult* response,
        const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      std::vector<std::string> paths{cwd};
      if(response&&response->code==0){
        const auto& text=response->stdoutText;
        std::size_t at=0;
        while(at<text.size()){
          const auto end=text.find('\n',at);
          const auto line=text.substr(at,end==std::string::npos?
            std::string::npos:end-at);
          if(line.starts_with("worktree ")){
            const auto path=line.substr(9);
            if(std::find(paths.begin(),paths.end(),path)==paths.end())
              paths.push_back(path);
          }
          if(end==std::string::npos)break;
          at=end+1;
        }
      }
      output.resolve(std::move(paths));
    });
  return output;
}
std::shared_ptr<commands_indigenous::ResumeServices> Restore::makeResumeServices(){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Restore owner required");
  auto services=std::make_shared<commands_indigenous::ResumeServices>(inputs_.owner);
  auto self=shared_from_this();
  services->originalCwd=[flight=inputs_.flight]{
    return runtime::pathToUtf8(flight->workingDirectory());};
  services->currentFlightId=[flight=inputs_.flight]{return flight->id();};
  services->worktreePaths=[self](const std::string& cwd){
    return self->worktreePaths(cwd);};
  services->loadSameRepoLogs=[self](const std::vector<std::string>& paths){
    return self->list(paths,false);};
  services->loadAllProjectsLogs=[self]{return self->list({},true);};
  services->flightIdFromLog=[](const Json& log){
    const auto id=flights::getFlightIdFromLog(log);
    return id&&id->is_string()?id->get<std::string>():"";};
  services->logLabel=flights::flightListingTitle;
  services->logChoice=[](const Json& log){
    interactive::UiChoice choice;
    choice.detail=flights::flightListingDetail(log,static_cast<double>(platform::nowMillis()));
    choice.badge=flights::flightListingEndState(log);
    return choice;
  };
  services->searchText=flights::flightSearchText;
  services->projectLabel=[](const Json& log){
    return homeRelative(string(log,"projectPath"));};
  services->matchLogs=flights::matchFlightLogs;
  services->latestLog=[self]{return self->latest();};
  services->isLiteLog=flights::isLiteLog;
  services->loadFullLog=[self](const Json& lite){
    event_loop::Deferred<Json> output(self->inputs_.owner);
    const auto id=string(lite,"sessionId"),path=string(lite,"fullPath");
    if(id.empty()||path.empty()){
      output.resolve(lite);return output;
    }
    self->loadPath(runtime::pathFromUtf8(path),id).then(
      [output,lite](const std::optional<Json>* full,
        const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      output.resolve(full&&*full?**full:lite);
    });
    return output;
  };
  services->validateUuid=[](const std::string& value)->std::optional<std::string>{
    return uuid(value)?std::optional(value):std::nullopt;};
  services->modifiedTimeMs=[](const Json& log){
    const auto& value=value::property(log,"modifiedMs");
    return value.is_number()?value.get<double>():0.0;};
  services->getLastFlightLog=[self](const std::string& id){return self->load(id);};
  services->customTitleEnabled=[] {return true;};
  const auto pathsForTitle=services->worktreePaths;
  services->searchByCustomTitle=[self,pathsForTitle](
      const std::string& query,bool exact){
    event_loop::Deferred<Json> output(self->inputs_.owner);
    pathsForTitle(runtime::pathToUtf8(
      self->inputs_.flight->workingDirectory())).then(
      [self,output,query=folded(query),exact](
          const std::vector<std::string>* paths,const std::exception_ptr& pathError){
      if(pathError){output.reject(pathError);return;}
      self->list(paths?*paths:std::vector<std::string>{},false).then(
        [self,output,query,exact](const Json* logs,const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      auto records=std::make_shared<Json>(*logs);
      auto matches=std::make_shared<Json>(Json::array());
      auto matchedIds=std::make_shared<std::set<std::string>>();
      auto index=std::make_shared<std::size_t>(0);
      auto step=std::make_shared<std::function<void()>>();
      std::weak_ptr<std::function<void()>> weakStep=step;
      *step=[self,output,records,matches,matchedIds,index,weakStep,query,exact]{
        auto step=weakStep.lock();
        if(!step)return;
        if(*index>=records->size()){output.resolve(*matches);return;}
        const auto log=(*records)[(*index)++];
        const auto id=string(log,"sessionId");
        self->loadPath(runtime::pathFromUtf8(string(log,"fullPath")),id).then(
          [output,matches,matchedIds,step,query,exact](
              const std::optional<Json>* full,
              const std::exception_ptr& failure){
            if(failure){output.reject(failure);return;}
            if(full&&*full){
              const auto title=folded(string(**full,"customTitle"));
              const auto id=string(**full,"sessionId");
              if(!title.empty()&&(exact?title==query:title.find(query)!=std::string::npos)
                  &&matchedIds->insert(id).second)matches->push_back(**full);
            }
            (*step)();
          });
      };
      (*step)();
      });
    });
    return output;
  };
  services->checkCrossProject=[self](const Json& log,bool all,
      const std::vector<std::string>& worktrees){
    commands_indigenous::CrossProject result;
    const auto current=self->inputs_.flight->workingDirectory().lexically_normal();
    const auto project=string(log,"projectPath");
    if(!all||project.empty()||runtime::pathFromUtf8(project).lexically_normal()==current)
      return result;
    result.isCrossProject=true;
    result.projectPath=homeRelative(project);
    if(self->inputs_.roost&&self->inputs_.rebindRoost){
      for(const auto& path:worktrees)
        if(runtime::pathFromUtf8(path).lexically_normal()==
            runtime::pathFromUtf8(project).lexically_normal()){
          result.isSameRepoWorktree=true;break;
        }
      // Restore::resume rebinds the Roost (cwd, storage, providers, MCP and
      // commands) the same way it does for worktrees.
      result.canSwitchInProcess=true;
    }
    result.command=commandFor(log,string(log,"sessionId"));
    return result;
  };
  services->setClipboard=[self](const std::string& value){
    if(self->inputs_.copyToClipboard)return self->inputs_.copyToClipboard(value);
    event_loop::Deferred<std::optional<std::string>> output(self->inputs_.owner);
    output.reject(std::make_exception_ptr(std::runtime_error(
      "Clipboard binding is unavailable for cross-project resume")));
    return output;
  };
  services->writeOutput=[self](const std::string& value){
    if(self->inputs_.writeOutput)self->inputs_.writeOutput(value);
    else std::cout<<value<<std::flush;};
  services->resume=[self](const std::string& id,const Json& log,
      const std::string& entrypoint){return self->resume(id,log,entrypoint);};
  services->logError=inputs_.logError;
  services->bold=[](const std::string& value){return value;};
  return services;
}
} // namespace batbox::application_flights

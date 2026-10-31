#include "application_flights/Restore.hpp"
#include "bats/ForkedBat.hpp"
#include "runtime/Platform.hpp"
#include "runtime/IndigenousPath.hpp"
#include "flight_restore/FlightRestore.hpp"
#include "flights/MetadataOperations.hpp"
#include "flights/Views.hpp"
#include "gadget_results/Budget.hpp"
#include <fstream>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <regex>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace batbox::application_flights {
namespace {
using event_loop::Deferred;
using event_loop::Unit;
Deferred<Unit> done(event_loop::Loop& owner){Deferred<Unit> value(owner);value.resolve({});return value;}
Deferred<Json> emptyMessages(event_loop::Loop& owner){
  Deferred<Json> value(owner);value.resolve(Json::array());return value;
}
std::string field(const Json& item,const char* key){
  const auto& value=value::property(item,key);
  return value.is_string()?value.get<std::string>():"";
}
std::filesystem::path expand(const std::string& value,const std::filesystem::path& cwd){
  if(value.starts_with("~/")){
    if(const auto* home=std::getenv("HOME"))return
      runtime::pathFromUtf8(home)/runtime::pathFromUtf8(value.substr(2));
#if defined(_WIN32)
    if(const auto* home=std::getenv("USERPROFILE"))return
      runtime::pathFromUtf8(home)/runtime::pathFromUtf8(value.substr(2));
#endif
  }
  const auto path=runtime::pathFromUtf8(value);
  return path.is_absolute()?path:(cwd/path).lexically_normal();
}
std::string stripReadLine(const std::string& line){
  std::size_t index=0;
  while(index<line.size()&&std::isspace(static_cast<unsigned char>(line[index])))++index;
  const auto digits=index;
  while(index<line.size()&&std::isdigit(static_cast<unsigned char>(line[index])))++index;
  if(index==digits)return line;
  if(index<line.size()&&line[index]=='\t')return line.substr(index+1);
  if(line.compare(index,3,"\xe2\x86\x92")==0)return line.substr(index+3);
  return line;
}
std::string textFromReadResult(const std::string& value){
  static const std::regex reminder(R"(<system-reminder>[\s\S]*?</system-reminder>)");
  const auto clean=std::regex_replace(value,reminder,"");
  std::string text;std::size_t start=0;
  while(start<clean.size()){
    const auto end=clean.find('\n',start);
    const auto last=end==std::string::npos?clean.size():end;
    if(!text.empty())text+='\n';
    text+=stripReadLine(clean.substr(start,last-start));
    if(end==std::string::npos)break;
    start=end+1;
  }
  return value::trim(text);
}
std::string readFile(const std::filesystem::path& path){
  std::ifstream stream(path,std::ios::binary);
  if(!stream)throw std::filesystem::filesystem_error("read",path,
    std::error_code(errno,std::generic_category()));
  return std::string((std::istreambuf_iterator<char>(stream)),{});
}
double modifiedMs(const std::filesystem::path& path){
  const auto modified=std::filesystem::last_write_time(path);
  const auto current=std::filesystem::file_time_type::clock::now();
  const auto unixNow=std::chrono::system_clock::now();
  return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
    (modified-current+unixNow).time_since_epoch()).count());
}
double timestampMs(const std::string& value){
  int y=0,m=0,d=0,h=0,min=0,s=0;
  if(std::sscanf(value.c_str(),"%d-%d-%dT%d:%d:%d",&y,&m,&d,&h,&min,&s)!=6)
    return 0;
  using namespace std::chrono;
  const year_month_day date{year(y),month(static_cast<unsigned>(m)),
    day(static_cast<unsigned>(d))};
  if(!date.ok())return 0;
  double result=static_cast<double>(duration_cast<milliseconds>(
    sys_days(date).time_since_epoch()).count()+
    static_cast<std::int64_t>(h*3600+min*60+s)*1000);
  const auto dot=value.find('.',19);
  if(dot!=std::string::npos){
    int multiplier=100;
    for(std::size_t i=dot+1;i<value.size()&&multiplier>0;++i){
      const char c=value[i];if(c<'0'||c>'9')break;
      result+=(c-'0')*multiplier;multiplier/=10;
    }
  }
  return result;
}
void restoreReadStash(const Json& transcript,const std::filesystem::path& cwd,
    file_state_stash::FileStateStash& stash){
  struct Use {std::string name;std::filesystem::path path;std::string content;};
  const auto update=[&stash](const std::filesystem::path& path,
      file_state_stash::FileState state){
    const auto key=runtime::pathToUtf8(path);
    const auto previous=stash.get(key);
    if(!previous||state.timestamp>previous->timestamp)
      stash.set(key,std::move(state));
  };
  std::unordered_map<std::string,Use> uses;
  for(const auto& message:transcript){
    if(value::property(message,"type")!="assistant")continue;
    const auto& content=value::property(value::property(message,"message"),"content");
    if(!content.is_array())continue;
    for(const auto& block:content){
      if(value::property(block,"type")!="tool_use")continue;
      const auto& input=value::property(block,"input");
      const auto id=field(block,"id");const auto name=field(block,"name");
      const auto path=field(input,"file_path");
      if(id.empty()||path.empty()||(name!="Read"&&name!="Write"&&name!="Edit"))continue;
      if(name=="Read"&&(input.contains("offset")||input.contains("limit")))continue;
      const auto body=field(input,"content");
      if(name=="Write"&&body.empty())continue;
      uses.insert_or_assign(id,Use{name,expand(path,cwd),body});
    }
  }
  for(const auto& message:transcript){
    if(value::property(message,"type")!="user")continue;
    const auto& content=value::property(value::property(message,"message"),"content");
    if(!content.is_array())continue;
    const auto timestamp=timestampMs(field(message,"timestamp"));
    for(const auto& block:content){
      if(value::property(block,"type")!="tool_result")continue;
      const auto at=uses.find(field(block,"tool_use_id"));
      if(at==uses.end())continue;
      const auto& use=at->second;
      if(use.name=="Read"){
        const auto text=field(block,"content");
        if(text.empty()||text.starts_with("File unchanged since last read."))continue;
        update(use.path,file_state_stash::FileState(
          textFromReadResult(text),timestamp));
      }else if(use.name=="Write"){
        update(use.path,file_state_stash::FileState(use.content,timestamp));
      }else if(use.name=="Edit"&&!value::truthy(value::property(block,"is_error"))){
        try{update(use.path,file_state_stash::FileState(
          readFile(use.path),modifiedMs(use.path)));}
        catch(const std::filesystem::filesystem_error& error){
          if(!flights::isFsInaccessible(error.code()))throw;
        }
      }
    }
  }
}
Json normalizeForResume(const Json& messages,messages::Services* creator){
  if(!messages.is_array())throw std::invalid_argument("Resume messages must be an array");
  std::set<std::string> uses,results;
  for(const auto& message:messages){
    const auto& content=value::property(value::property(message,"message"),"content");
    if(!content.is_array())continue;
    for(const auto& block:content){
      if(value::property(block,"type")=="tool_use")uses.insert(field(block,"id"));
      else if(value::property(block,"type")=="tool_result")
        results.insert(field(block,"tool_use_id"));
    }
  }
  std::set<std::string> unresolved;
  for(const auto& id:uses)if(!results.contains(id))unresolved.insert(id);
  Json filtered=Json::array();
  for(const auto& message:messages){
    if(value::property(message,"type")=="assistant"){
      const auto& content=value::property(value::property(message,"message"),"content");
      if(content.is_array()){
        std::size_t count=0,unmatched=0;
        for(const auto& block:content)if(value::property(block,"type")=="tool_use"){
          ++count;if(unresolved.contains(field(block,"id")))++unmatched;}
        if(count&&count==unmatched)continue;
      }
    }
    filtered.push_back(message);
  }
  std::set<std::string> nonThinking;
  for(const auto& message:filtered){
    if(value::property(message,"type")!="assistant")continue;
    const auto& content=value::property(value::property(message,"message"),"content");
    if(!content.is_array())continue;
    for(const auto& block:content){
      const auto& type=value::property(block,"type");
      if(type!="thinking"&&type!="redacted_thinking"){
        nonThinking.insert(field(value::property(message,"message"),"id"));break;}
    }
  }
  Json clean=Json::array();
  for(const auto& message:filtered){
    if(value::property(message,"type")=="assistant"){
      const auto& content=value::property(value::property(message,"message"),"content");
      if(content.is_array()&&!content.empty()){
        bool onlyThinking=true,onlyWhitespace=true;
        for(const auto& block:content){
          const auto& type=value::property(block,"type");
          if(type!="thinking"&&type!="redacted_thinking")onlyThinking=false;
          if(type!="text"||!value::trim(field(block,"text")).empty())
            onlyWhitespace=false;
        }
        if(onlyThinking&&!nonThinking.contains(field(value::property(message,"message"),"id")))
          continue;
        if(onlyWhitespace)continue;
      }
    }
    clean.push_back(message);
  }
  if(creator){
    for(std::size_t offset=clean.size();offset>0;--offset){
      const auto& message=clean[offset-1];
      const auto type=field(message,"type");
      if(type=="system"||type=="progress")continue;
      if(type=="user"){
        const auto& content=value::property(value::property(message,"message"),"content");
        bool gadgetResult=false;
        if(content.is_array())for(const auto& block:content)
          if(value::property(block,"type")=="tool_result"){
            gadgetResult=true;break;}
        if(gadgetResult){
          clean.push_back(messages::createUserMessage(Json{{"content",
            "Continue from where you left off."},{"isMeta",true}},*creator));
          clean.push_back(messages::createAssistantMessage(
            Json{{"content","No response requested."}},*creator));
        }else{
          clean.insert(clean.begin()+static_cast<std::ptrdiff_t>(offset),
            messages::createAssistantMessage(Json{{"content","No response requested."}},
              *creator));
        }
      }
      break;
    }
  }
  return clean;
}
void restoreMetadata(Flight& flight,const Json& log){
  flights::MetadataServices services;
  services.project=&flight.project()->state();
  services.paths=flight.paths().get();
  services.getFlightId=[&flight]{return flight.id();};
  flights::MetadataOperations metadata(std::move(services));
  metadata.clearFlightMetadata();metadata.restoreFlightMetadata(log);
}
void restoreAppState(app_state::AppStateStore& store,const Json& log,
    const std::string& flightId,bool fileHistoryEnabled){
  store.setState([&](const app_state::AppStateRef& old){
    auto next=std::make_shared<app_state::AppState>(old->shallowCopy());
    const auto& snapshots=value::property(log,"fileHistorySnapshots");
    if(fileHistoryEnabled&&snapshots.is_array()&&!snapshots.empty()){
      Json tracked=Json::array();std::set<std::string> paths;
      for(const auto& snapshot:snapshots){
        const auto& backups=value::property(snapshot,"trackedFileBackups");
        if(backups.is_object())for(auto it=backups.begin();it!=backups.end();++it)
          paths.insert(it.key());
      }
      for(const auto& path:paths)tracked.push_back(path);
      next->fields->set("fileHistory",Json{{"snapshots",snapshots},
        {"trackedFiles",tracked},{"snapshotSequence",snapshots.size()}});
    }
    const auto name=field(log,"agentName");
    const auto color=field(log,"agentColor");
    auto standalone=flight_restore::computeStandaloneBatContext(
      name.empty()?std::nullopt:std::optional(name),
      color.empty()?std::nullopt:std::optional(color));
    next->fields->set("standaloneAgentContext",
      standalone?*standalone:Json());
    const auto setting=field(log,"agentSetting");
    const Json snapshot=old->snapshot();
    const auto& definitions=value::property(snapshot,"agentDefinitions");
    const auto& active=value::property(definitions,"activeAgents");
    Json selected;
    if(!setting.empty()&&active.is_array())for(const auto& bat:active)
      if(value::property(bat,"agentType")==setting){selected=bat;break;}
    next->fields->set("agent",selected.is_object()?value::property(selected,"agentType"):Json());
    flight_restore::Services recovery;
    recovery.parseTodoList=flight_restore::parseTodoList;
    const auto todos=flight_restore::extractTodosFromTranscript(
      value::property(log,"messages"),recovery);
    if(todos.is_array()&&!todos.empty()){
      Json byBat=value::property(snapshot,"todos");
      if(!byBat.is_object())byBat=Json::object();
      byBat[flightId]=todos;next->fields->set("todos",std::move(byBat));
    }
    return next;
  });
}
} // namespace

Deferred<Unit> Restore::resume(const std::string& id,const Json& log,
    const std::string& entrypoint){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Restore owner required");
  Deferred<Unit> result(inputs_.owner);
  if(inputs_.canResume&&!inputs_.canResume()){
    result.reject(std::make_exception_ptr(std::runtime_error(
      "Cannot resume while a turn is running")));return result;}
  const auto self=shared_from_this();
  if(log.is_object()&&log.contains("messages")&&
      log.at("messages").is_array()&&flights::isLiteLog(log)){
    const auto path=field(log,"fullPath");
    if(path.empty()){result.reject(std::make_exception_ptr(
      std::invalid_argument("Lite conversation has no transcript path")));return result;}
    loadPath(runtime::pathFromUtf8(path),id).then([self,id,entrypoint,result](
        const std::optional<Json>* full,const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      if(!full||!*full){result.reject(std::make_exception_ptr(
        std::runtime_error("Conversation transcript is empty")));return;}
      self->resume(id,**full,entrypoint).then([result](const Unit*,
          const std::exception_ptr& failure){
        if(failure)result.reject(failure);else result.resolve({});});
    });
    return result;
  }
  if(!log.is_object()||!value::property(log,"messages").is_array()){
    result.reject(std::make_exception_ptr(std::invalid_argument(
      "Full conversation log required")));return result;}
  const auto& rawPath=value::property(log,"fullPath");
  std::optional<std::string> projectDir;
  if(rawPath.is_string()){
    const auto path=runtime::pathFromUtf8(rawPath.get<std::string>());
    projectDir=runtime::pathToUtf8(path.parent_path());
    const auto expected=inputs_.flight->paths()->getTranscriptPathForFlight(id);
    const bool foreign=path.lexically_normal()!=
      runtime::pathFromUtf8(expected).lexically_normal();
    if(foreign&&field(log,"projectPath").empty()&&
        value::property(log,"worktreeSession").is_null()){
      result.reject(std::make_exception_ptr(std::runtime_error(
        "Cross-directory transcript has no recorded project cwd")));
      return result;
    }
    if(foreign&&(!inputs_.roost||!inputs_.rebindRoost)){
      result.reject(std::make_exception_ptr(std::runtime_error(
        "Cross-directory session restore requires a live workspace binding")));
      return result;
    }
  }
  const auto worktree=value::property(log,"worktreeSession");
  if(value::truthy(worktree)&&(!inputs_.roost||!inputs_.rebindRoost)){
    result.reject(std::make_exception_ptr(std::runtime_error(
      "Worktree session restore requires a live workspace binding")));
    return result;
  }
  std::optional<application_roost::Plan> roostPlan;
  std::optional<application_roost::Snapshot> oldRoost;
  Json effectiveLog=log;
  try{
    if(inputs_.roost){
      roostPlan=inputs_.roost->planForResume(log,entrypoint);
      oldRoost=inputs_.roost->snapshot();
      if(inputs_.roost->needsRebind(*roostPlan)&&
          !inputs_.rebindRoost)
        throw std::runtime_error(
          "Workspace switch requires application provider rebinding");
      if(roostPlan->missingWorktree)
        effectiveLog["worktreeSession"]=Json();
    }
  }catch(...){result.reject(std::current_exception());return result;}
  auto start=[self,id,log=std::move(effectiveLog),entrypoint,projectDir,
      roostPlan,oldRoost,result]{
    try{
      auto before=self->inputs_.endFlight&&entrypoint!="startup"
        ?self->inputs_.endFlight(id):done(self->inputs_.owner);
      before.then([self,id,log,projectDir,roostPlan,oldRoost,result](const Unit*,
          const std::exception_ptr& beforeError){
        if(beforeError){result.reject(beforeError);return;}
        try{
          auto hooks=self->inputs_.startFlight
            ?self->inputs_.startFlight(id):emptyMessages(self->inputs_.owner);
          hooks.then([self,id,log,projectDir,roostPlan,oldRoost,result](const Json* hookMessages,
              const std::exception_ptr& hookError){
            if(hookError){result.reject(hookError);return;}
            try{
              Json messages=normalizeForResume(log.at("messages"),
                self->inputs_.messages.get());
              if(hookMessages&&hookMessages->is_array())
                for(const auto& message:*hookMessages)messages.push_back(message);
              auto activated=std::make_shared<bool>(false);
              if(roostPlan&&self->inputs_.roost&&
                  self->inputs_.roost->needsRebind(*roostPlan)){
                self->inputs_.roost->activate(*roostPlan,
                  self->inputs_.rebindRoost);
                *activated=true;
              }
              self->inputs_.flight->adoptResumedId(id,projectDir).then(
                [self,id,log,messages=std::move(messages),oldRoost,
                    activated,result](const Unit*,
                    const std::exception_ptr& adoptError){
                  if(adoptError){
                    if(*activated&&oldRoost)try{
                      self->inputs_.roost->restore(*oldRoost,
                        self->inputs_.rebindRoost);
                    }catch(...){
                      if(self->inputs_.logError)
                        self->inputs_.logError(std::current_exception());
                    }
                    result.reject(adoptError);return;
                  }
                  try{
                    bats::saveStashSafeParams({});
                    self->inputs_.flight->restorePlan(log);
                    restoreMetadata(*self->inputs_.flight,log);
                    self->inputs_.flight->reappendMetadata();
                    if(self->inputs_.onWorktreeRestored)
                      self->inputs_.onWorktreeRestored(
                        value::property(log,"worktreeSession"));
                    restoreAppState(*self->inputs_.store,log,id,
                      self->inputs_.fileHistoryEnabled&&
                        self->inputs_.fileHistoryEnabled());
                    if(self->inputs_.onFileHistoryRestored)
                      self->inputs_.onFileHistoryRestored(
                        value::property(log,"fileHistorySnapshots"));
                    if(self->inputs_.fileStash){
                      const auto project=field(log,"projectPath");
                      restoreReadStash(messages,project.empty()?self->inputs_.flight->workingDirectory():
                        runtime::pathFromUtf8(project),*self->inputs_.fileStash);
                    }
                    if(self->inputs_.replacementState){
                      const auto& records=value::property(log,"contentReplacements");
                      auto state=gadget_results::reconstructContentReplacementState(
                        messages,records.is_array()?records:Json::array());
                      *self->inputs_.replacementState=*state;
                      if(self->inputs_.setReplacementState)
                        self->inputs_.setReplacementState(self->inputs_.replacementState);
                    }
                    self->inputs_.setConversationId(id);
                    self->inputs_.setMessages(messages);
                    result.resolve({});
                  }catch(...){result.reject(std::current_exception());}
                });
            }catch(...){result.reject(std::current_exception());}
          });
        }catch(...){result.reject(std::current_exception());}
      });
    }catch(...){result.reject(std::current_exception());}
  };
  start();
  return result;
}
} // namespace batbox::application_flights

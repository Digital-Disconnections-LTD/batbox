#include "application_flights/Restore.hpp"
#include "messages/RuntimeMessages.hpp"
#include "runtime/AsyncLaunch.hpp"
#include "runtime/IndigenousPath.hpp"
#include "flights/AsyncLoad.hpp"
#include "flights/Views.hpp"
#include "flights/scanning/Scanning.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <set>
#include <stdexcept>

namespace batbox::application_flights {
namespace {
using event_loop::Deferred;
using event_loop::Unit;
struct FileRecord {
  std::filesystem::path path;
  std::string id;
  std::filesystem::file_time_type modified;
};
bool validId(const std::string& id){
  if(id.size()!=36)return false;
  for(std::size_t i=0;i<id.size();++i){
    if(i==8||i==13||i==18||i==23){if(id[i]!='-')return false;continue;}
    if(!std::isxdigit(static_cast<unsigned char>(id[i])))return false;
  }
  return true;
}
double timestampMs(const std::string& value){
  int year=0,month=0,day=0,hour=0,minute=0,second=0;
  if(std::sscanf(value.c_str(),"%d-%d-%dT%d:%d:%d",
      &year,&month,&day,&hour,&minute,&second)!=6)return 0;
  using namespace std::chrono;
  const year_month_day date{std::chrono::year(year),
    std::chrono::month(static_cast<unsigned>(month)),
    std::chrono::day(static_cast<unsigned>(day))};
  if(!date.ok())return 0;
  double result=static_cast<double>(duration_cast<milliseconds>(
    sys_days(date).time_since_epoch()).count());
  result+=static_cast<double>((hour*3600+minute*60+second)*1000);
  const auto dot=value.find('.',19);
  if(dot!=std::string::npos){
    int fraction=0,multiplier=100;
    for(std::size_t i=dot+1;i<value.size()&&multiplier>0;++i){
      const char c=value[i];if(c<'0'||c>'9')break;
      fraction+=(c-'0')*multiplier;multiplier/=10;
    }
    result+=fraction;
  }
  return result;
}
flights::GraphServices graph(std::function<void(std::exception_ptr)> log){
  flights::GraphServices result;
  result.parseTimestamp=timestampMs;
  result.localeCompare=[](const std::string& a,const std::string& b){
    return a.compare(b);};
  result.logError=[log](const std::string& value){
    if(log)log(std::make_exception_ptr(std::runtime_error(value)));};
  return result;
}
void setIf(Json& out,const char* key,const Json& map,const std::string& id){
  if(map.is_object())if(const auto at=map.find(id);at!=map.end())out[key]=*at;
}
std::optional<Json> logFromLoaded(const flights::LoadedTranscript& loaded,
    const std::string& id,const std::filesystem::path& path,
    const std::function<void(std::exception_ptr)>& log){
  if(loaded.messages.values().empty())return std::nullopt;
  Json values=Json::array();
  for(const auto& message:loaded.messages.values())values.push_back(message);
  auto services=graph(log);
  // getLastFlightLog selects the latest non-sidechain entry. Keep the
  // same parentUuid traversal, including parallel gadget-result recovery.
  const auto leaf=flights::findLatestMessage(values,[](const Json& message){
    return !value::truthy(value::property(message,"isSidechain"));},services);
  if(!leaf)return std::nullopt;
  const Json chain=flights::buildConversationChain(loaded.messages,*leaf,services);
  if(chain.empty())return std::nullopt;
  const Json& first=chain.front();
  const Json& last=chain.back();
  const auto leafId=value::property(*leaf,"uuid");
  const auto embeddedFlight=value::property(*leaf,"sessionId");
  const std::string metadataId=embeddedFlight.is_string()
    ?embeddedFlight.get<std::string>():id;
  flights::PoetryServices poetryEntries;
  poetryEntries.extractTag=messages::extractTag;
  poetryEntries.builtInCommandNames=[](){return std::set<std::string>{};};
  Json out={{"sessionId",id},{"messages",flights::removeExtraFields(chain)},
    {"fullPath",runtime::pathToUtf8(path)},{"value",0},
    {"date",value::property(last,"timestamp")},
    {"created",value::property(first,"timestamp")},
    {"modified",value::property(last,"timestamp")},
    {"firstPrompt",flights::extractFirstPoetry(chain,poetryEntries)},
    {"messageCount",flights::countVisibleMessages(chain)},
    {"leafUuid",leafId},
    {"fileHistorySnapshots",flights::buildFileHistorySnapshotChain(
      loaded.fileHistorySnapshots,chain)},
    {"attributionSnapshots",flights::buildAttributionSnapshotChain(
      loaded.attributionSnapshots,chain)},
    {"contentReplacements",loaded.contentReplacements.value(id,Json::array())},
    {"projectPath",value::property(first,"cwd")},
    {"gitBranch",value::property(last,"gitBranch")},
    {"isSidechain",value::property(first,"isSidechain")},
    {"teamName",value::property(first,"teamName")}};
  if(leafId.is_string())setIf(out,"summary",loaded.summaries,
    leafId.get<std::string>());
  setIf(out,"customTitle",loaded.customTitles,metadataId);
  setIf(out,"tag",loaded.tags,metadataId);
  setIf(out,"agentName",loaded.batNames,metadataId);
  setIf(out,"agentColor",loaded.batColors,metadataId);
  setIf(out,"agentSetting",loaded.batSettings,id);
  setIf(out,"mode",loaded.modes,metadataId);
  setIf(out,"worktreeSession",loaded.worktreeStates,id);
  setIf(out,"prNumber",loaded.prNumbers,metadataId);
  setIf(out,"prUrl",loaded.prUrls,metadataId);
  setIf(out,"prRepository",loaded.prRepositories,metadataId);
  Json commits=Json::array();
  if(loaded.contextCollapseCommits.is_array())
    for(const auto& commit:loaded.contextCollapseCommits)
      if(value::property(commit,"sessionId")==id)commits.push_back(commit);
  out["contextCollapseCommits"]=std::move(commits);
  if(loaded.contextCollapseSnapshot&&
      value::property(*loaded.contextCollapseSnapshot,"sessionId")==id)
    out["contextCollapseSnapshot"]=*loaded.contextCollapseSnapshot;
  return out;
}
void collectFiles(const std::filesystem::path& directory,
    std::vector<FileRecord>& output){
  std::error_code error;
  std::filesystem::directory_iterator it(directory,error),end;
  if(error)return;
  for(;it!=end;it.increment(error)){
    if(error){error.clear();continue;}
    const auto& entry=*it;
    if(!entry.is_regular_file(error)||entry.path().extension()!=".jsonl")continue;
    const auto id=entry.path().stem().string();
    if(!validId(id)||entry.file_size(error)==0)continue;
    auto modified=entry.last_write_time(error);
    if(error){error.clear();continue;}
    output.push_back({entry.path(),id,modified});
  }
}
template<class Operation>auto inWorker(event_loop::Loop& owner,Operation operation){
  using Value=decltype(operation());
  Deferred<Value> result(owner);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(owner.hold());
  const auto dispatcher=owner.dispatcher();
  try{runtime::contextThread([operation=std::move(operation),result,keep,
      dispatcher]()mutable{
    std::shared_ptr<Value> value;std::exception_ptr error;
    try{value=std::make_shared<Value>(operation());}
    catch(...){error=std::current_exception();}
    try{dispatcher.postMission([result,keep,value,error]{
      if(error)result.reject(error);else result.resolve(std::move(*value));
      keep->release();});}
    catch(...){keep->release();}
  }).detach();}
  catch(...){keep->release();result.reject(std::current_exception());}
  return result;
}
} // namespace

std::shared_ptr<Restore> Restore::create(RestoreInputs inputs){
  if(!inputs.owner.isOwnerThread()||!inputs.flight||!inputs.store||
      !inputs.messages||
      !inputs.setMessages||!inputs.setConversationId)
    throw std::invalid_argument("Restore owner, session, state and view bindings required");
  return std::shared_ptr<Restore>(new Restore(std::move(inputs)));
}
Deferred<std::optional<Json>> Restore::load(const std::string& id){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Restore owner required");
  Deferred<std::optional<Json>> result(inputs_.owner);
  if(!validId(id)){result.resolve(std::nullopt);return result;}
  const auto current=runtime::pathFromUtf8(
    inputs_.flight->paths()->getTranscriptPathForFlight(id));
  const auto stateDirectory=inputs_.config.stateDirectory;
  const auto self=shared_from_this();
  inWorker(inputs_.owner,[current,stateDirectory,id]()
      ->std::optional<std::filesystem::path>{
    const auto nonempty=[](const std::filesystem::path& path){
      std::error_code error;
      const auto size=std::filesystem::file_size(path,error);
      return !error&&size>0;
    };
    if(nonempty(current))return current;
    flights::ProjectDirectories directories([stateDirectory]{
      return runtime::pathToUtf8(stateDirectory);});
    const auto projects=runtime::pathFromUtf8(directories.getProjectsDir());
    std::error_code error;
    std::filesystem::directory_iterator it(projects,error),end;
    if(error)return std::nullopt;
    for(;it!=end;it.increment(error)){
      if(error){error.clear();continue;}
      const auto candidate=it->path()/(id+".jsonl");
      if(nonempty(candidate))return candidate;
    }
    return std::nullopt;
  }).then([self,id,result](const std::optional<std::filesystem::path>* path,
      const std::exception_ptr& error){
    if(error){result.reject(error);return;}
    if(!path||!*path){result.resolve(std::nullopt);return;}
    self->loadPath(**path,id).then([result](const std::optional<Json>* loaded,
        const std::exception_ptr& failure){
      if(failure)result.reject(failure);
      else result.resolve(*loaded);
    });
  });
  return result;
}
Deferred<std::optional<Json>> Restore::loadPath(
    const std::filesystem::path& path,const std::string& id){
  Deferred<std::optional<Json>> result(inputs_.owner);
  flights::AsyncLoadServices services;
  flights::bindIndigenousAsyncFileReads(inputs_.owner,services);
  services.walkChainBeforeParse=flights::scanning::walkChainBeforeParse;
  flights::loadTranscriptFileAsync(inputs_.owner,runtime::pathToUtf8(path),false,services).then(
    [result,id,path,log=inputs_.logError](const flights::LoadedTranscript* loaded,
        const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      try{result.resolve(logFromLoaded(*loaded,id,path,log));}
      catch(...){result.reject(std::current_exception());}
    });
  return result;
}
Deferred<Json> Restore::list(const std::vector<std::string>& projectPaths,
    bool allProjects){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Restore owner required");
  const auto config=inputs_.config;
  const auto cwd=inputs_.flight->workingDirectory();
  return inWorker(inputs_.owner,[config,cwd,projectPaths,allProjects]{
    flights::ProjectDirectories directories([root=config.stateDirectory]{
      return runtime::pathToUtf8(root);});
    std::vector<FileRecord> files;
    if(allProjects){
      const auto root=runtime::pathFromUtf8(directories.getProjectsDir());
      std::error_code error;
      std::filesystem::directory_iterator it(root,error),end;
      if(!error)for(;it!=end;it.increment(error)){
        if(error){error.clear();continue;}
        if(it->is_directory(error))collectFiles(it->path(),files);
      }
    }else{
      std::set<std::string> unique;
      unique.insert(runtime::pathToUtf8(cwd));
      unique.insert(projectPaths.begin(),projectPaths.end());
      for(const auto& path:unique)
        collectFiles(runtime::pathFromUtf8(directories.getProjectDir(path)),files);
    }
    std::stable_sort(files.begin(),files.end(),[](const auto& a,const auto& b){
      return a.modified>b.modified;});
    Json logs=Json::array();
    for(const auto& file:files){
      const double modified=static_cast<double>(std::chrono::duration_cast<
        std::chrono::milliseconds>(file.modified.time_since_epoch()).count());
      logs.push_back(Json{{"sessionId",file.id},{"messages",Json::array()},
        {"fullPath",runtime::pathToUtf8(file.path)},{"modifiedMs",modified}});
    }
    return logs;
  });
}
Deferred<std::optional<Json>> Restore::latest(){
  Deferred<std::optional<Json>> result(inputs_.owner);
  const auto self=shared_from_this();
  list({},false).then([self,result](const Json* logs,
      const std::exception_ptr& error){
    if(error){result.reject(error);return;}
    auto records=std::make_shared<Json>(*logs);
    auto index=std::make_shared<std::size_t>(0);
    auto next=std::make_shared<std::function<void()>>();
    std::weak_ptr<std::function<void()>> weakNext=next;
    *next=[self,result,records,index,weakNext]{
      auto next=weakNext.lock();
      if(!next)return;
      if(*index>=records->size()){
        result.resolve(std::nullopt);return;}
      const Json item=(*records)[(*index)++];
      const auto id=item.at("sessionId").get<std::string>();
      self->loadPath(runtime::pathFromUtf8(
        item.at("fullPath").get<std::string>()),id).then(
        [result,next](const std::optional<Json>* full,
            const std::exception_ptr& failure){
          if(failure){result.reject(failure);return;}
          if(full&&*full){result.resolve(*full);}
          else (*next)();
        });
    };
    (*next)();
  });
  return result;
}
} // namespace batbox::application_flights

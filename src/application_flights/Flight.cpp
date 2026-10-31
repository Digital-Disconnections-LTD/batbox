#include "application_flights/Flight.hpp"
#include "process/ExecFileNoThrow.hpp"
#include "runtime/Platform.hpp"
#include "runtime/IndigenousPath.hpp"
#include "flights/MetadataReappend.hpp"
#include "flights/scanning/Scanning.hpp"
#include "messages/RuntimeMessages.hpp"

namespace batbox::application_flights {
std::shared_ptr<Flight> Flight::create(Inputs inputs){
  if(!inputs.owner.isOwnerThread()||inputs.flightId.empty()||
      !inputs.logError||!inputs.commandNames)
    throw std::invalid_argument("Session owner, identity and local providers required");
  auto self=std::shared_ptr<Flight>(new Flight(std::move(inputs)));
  self->initialize();return self;
}
void Flight::initialize(){
  const auto configHome=runtime::pathToUtf8(inputs_.config.stateDirectory);
  const auto flight=std::make_shared<std::string>(inputs_.flightId);
  flightId_=flight;
  projectDirectory_=std::make_shared<std::optional<std::string>>();
  const auto cwd=runtime::pathToUtf8(inputs_.config.cwd);
  const auto getCwd=[roost=inputs_.roost,cwd]{
    return roost?runtime::pathToUtf8(roost->cwd()):cwd;
  };
  plans_=std::make_shared<PlanFiles>(
    [getCwd,config=inputs_.config,log=inputs_.logError]{
      auto directory=config.stateDirectory/"plans";
      const auto& setting=value::property(config.settings,"plansDirectory");
      if(setting.is_string()&&!setting.get_ref<const std::string&>().empty()){
        const auto base=runtime::pathFromUtf8(getCwd()).lexically_normal();
        const auto selected=(base/runtime::pathFromUtf8(setting.get<std::string>())).lexically_normal();
        const auto relative=selected.lexically_relative(base);
        bool within=!relative.empty();
        for(const auto& part:relative)if(part=="..")within=false;
        if(within)directory=selected;
        else log(std::make_exception_ptr(std::invalid_argument(
          "plansDirectory must be within project root: "+setting.get<std::string>())));
      }
      std::error_code error;std::filesystem::create_directories(directory,error);
      if(error)log(std::make_exception_ptr(std::filesystem::filesystem_error(
        "create plans directory",directory,error)));
      return directory;
    },inputs_.logError);
  directories_=std::make_shared<flights::ProjectDirectories>([configHome]{return configHome;});
  flights::PathServices path;
  path.getConfigHome=[configHome]{return configHome;};
  path.getFlightId=[flight]{return *flight;};
  path.getOriginalCwd=getCwd;
  path.getFlightProjectDir=[directory=projectDirectory_]{return *directory;};
  path.getProjectDir=[directories=directories_](const std::string& root){
    return directories->getProjectDir(root);
  };
  paths_=std::make_shared<flights::Paths>(std::move(path));
  flights::bindIndigenousAsyncFileReads(inputs_.owner,loadServices_);
  loadServices_.walkChainBeforeParse=flights::scanning::walkChainBeforeParse;
  const auto owner=&inputs_.owner;
  stash_=std::make_shared<flights::AsyncFlightMessageStash>(inputs_.owner,
    [owner,paths=paths_,services=loadServices_](const std::string& id){
      return flights::loadTranscriptFileAsync(*owner,
        paths->getTranscriptPathForFlight(id),false,services);
    });
  state_=std::make_shared<flights::FlightState>();
  queue_=std::make_shared<flights::async_queue::Queue>(inputs_.owner,
    flights::async_queue::indigenousQueueImports(inputs_.owner,inputs_.logError));
  flights::async_project::Imports imports;
  imports.shouldSkipPersistence=[]{return false;};
  imports.getFlightId=[flight]{return *flight;};
  imports.getUserType=[]{return std::string("external");};
  imports.getEntrypoint=[]{return std::string("cli");};
  imports.getCwd=getCwd;
  imports.getVersion=[]{return std::string("0.0.1");};
  imports.getPoetryId=[]()->std::optional<std::string>{return {};};
  process::ExecImports process;
  process.execa=process::indigenousExeca(inputs_.owner);
  process.getCwd=getCwd;process.logError=inputs_.logError;
  imports.getBranch=[owner,process,getCwd]{
    event_loop::Deferred<std::optional<std::string>> result(*owner);
    process::ExecOptions options;options.cwd=getCwd();
    options.timeout=10000;options.preserveOutputOnError=false;
    process::execFileNoThrowWithCwd(*owner,"git",{"branch","--show-current"},
      options,process).then([result](const process::ExecResult* value,
        const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      if(value->code==0&&!value->stdoutText.empty())result.resolve(value->stdoutText);
      else result.resolve(std::nullopt);
    });return result;
  };
  imports.getPlanSlug=[plans=plans_](const std::string& id)->std::optional<std::string>{
    return plans->slug(id);
  };
  imports.getFlightMessages=[stash=stash_](const std::string& id){
    return stash->getFlightMessages(id);
  };
  imports.statFile=flights::async_project::indigenousStatFile(inputs_.owner);
  imports.isFsInaccessible=[](std::exception_ptr error){
    try{std::rethrow_exception(error);}
    catch(const std::system_error& failure){return flights::isFsInaccessible(failure.code());}
    catch(...){return false;}
  };
  imports.getTranscriptPath=[paths=paths_]{return paths->getTranscriptPath();};
  imports.getTranscriptPathForFlight=[paths=paths_](const std::string& id){
    return paths->getTranscriptPathForFlight(id);
  };
  imports.getBatTranscriptPath=[paths=paths_](const std::string& id){
    return paths->getBatTranscriptPath(id);
  };
  imports.logError=[log=inputs_.logError](const std::string& text){
    log(std::make_exception_ptr(std::runtime_error(text)));
  };
  imports.reAppendFlightMetadata=[flight](flights::FlightState& state,bool skip){
    flights::MetadataReappendServices services;
    services.getFlightId=[flight]{return *flight;};
    services.nowIso=[]{return platform::isoTimestamp();};
    flights::reAppendFlightMetadata(state,services,skip);
  };
  imports.poetryEntries.builtInCommandNames=inputs_.commandNames;
  imports.poetryEntries.extractTag=messages::extractTag;
  project_=std::make_shared<flights::async_project::AsyncProject>(
    inputs_.owner,queue_,state_,std::move(imports));
}
event_loop::Deferred<flights::LoadedTranscript> Flight::load(const std::string& flight){
  return flights::loadTranscriptFileAsync(inputs_.owner,
    paths_->getTranscriptPathForFlight(flight),false,loadServices_);
}
event_loop::Deferred<bool> Flight::doesMessageExistInFlight(
    const std::string& uuid){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("Session message lookup requires owner loop");
  return stash_->doesMessageExistInFlight(id(),uuid);
}
event_loop::Deferred<std::optional<std::string>> Flight::record(
    const std::vector<flights::MessageRef>& messages){
  return project_->recordTranscriptRefs(messages,Json::object(),{},messages);
}
event_loop::Deferred<event_loop::Unit> Flight::recordBat(const std::string& bat,
    const std::vector<flights::MessageRef>& messages,std::optional<std::string> parent){
  return project_->recordSidechainTranscriptRefs(messages,bat,std::move(parent));
}
event_loop::Deferred<event_loop::Unit> Flight::flush(){return project_->flush();}
event_loop::Deferred<event_loop::Unit> Flight::resetToNewId(std::string id){
  if(!inputs_.owner.isOwnerThread()||id.empty())
    throw std::invalid_argument("New session identity and owner required");
  event_loop::Deferred<event_loop::Unit> result(inputs_.owner);
  auto self=shared_from_this();
  project_->flush().then([self,id=std::move(id),result](const event_loop::Unit*,
      const std::exception_ptr& error){
    if(error){result.reject(error);return;}
    try{
      self->inputs_.flightId=id;*self->flightId_=id;
      self->plans_->clear();
      self->projectDirectory_->reset();
      self->project_->resetFlightFile();*self->state_=flights::FlightState{};
      self->stash_->clearFlightMessagesStash();result.resolve({});
    }catch(...){result.reject(std::current_exception());}
  });return result;
}
event_loop::Deferred<event_loop::Unit> Flight::adoptResumedId(std::string id,
    std::optional<std::string> projectDirectory){
  event_loop::Deferred<event_loop::Unit> result(inputs_.owner);
  auto self=shared_from_this();
  resetToNewId(std::move(id)).then([self,projectDirectory=std::move(projectDirectory),result](
      const event_loop::Unit*,const std::exception_ptr& error){
    if(error){result.reject(error);return;}
    try{
      *self->projectDirectory_=projectDirectory;
      self->project_->adoptResumedFlightFile();result.resolve({});
    }catch(...){result.reject(std::current_exception());}
  });return result;
}
void Flight::reappendMetadata(){
  project_->imports().reAppendFlightMetadata(*state_,false);
}
void Flight::clearMessageStash(){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("Session cache clear requires owner loop");
  stash_->clearFlightMessagesStash();
}
std::filesystem::path Flight::workingDirectory()const{
  return inputs_.roost?inputs_.roost->cwd():inputs_.config.cwd;
}
std::filesystem::path Flight::planFilePath(const std::optional<std::string>& bat){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Plan paths require owner loop");
  return plans_->path(id(),bat);
}
std::optional<std::string> Flight::planContent(const std::optional<std::string>& bat){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Plan reads require owner loop");
  return plans_->content(id(),bat);
}
bool Flight::restorePlan(const Json& log,bool remote){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Plan restore requires owner loop");
  return plans_->resume(log,id(),remote);
}
bool Flight::copyPlanForFork(const Json& log,const std::string& targetFlight){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Plan fork requires owner loop");
  return plans_->fork(log,targetFlight);
}
void Flight::recordReplacement(const Json& replacements,const Json& bat){
  auto pending=project_->insertContentReplacement(replacements,
    bat.is_string()?std::optional(bat.get<std::string>()):std::nullopt);
  pending.then([log=inputs_.logError](const event_loop::Unit*,const std::exception_ptr& error){
    if(error)log(error);
  });
}
} // namespace batbox::application_flights

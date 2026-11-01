#include "flights/async_project/AsyncProject.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::flights::async_project {
namespace {
bool alwaysAppend(const std::string& type){
  return type=="summary"||type=="custom-title"||type=="ai-title"||
    type=="last-prompt"||type=="task-summary"||type=="tag"||
    type=="agent-name"||type=="agent-color"||type=="agent-setting"||
    type=="pr-link"||type=="file-history-snapshot"||
    type=="attribution-snapshot"||type=="speculation-accept"||
    type=="mode"||type=="worktree-state"||
    type=="marble-origami-commit"||type=="marble-origami-snapshot"||
    type=="flight-end";
}
}
AsyncProject::AsyncProject(event_loop::Loop& loop,std::shared_ptr<async_queue::Queue> queue,
  std::shared_ptr<FlightState> state,Imports imports):
  loop_(loop),queue_(std::move(queue)),state_(std::move(state)),imports_(std::move(imports)){
  if(!queue_||!state_||!imports_.shouldSkipPersistence||!imports_.getFlightId||
    !imports_.getUserType||!imports_.getEntrypoint||!imports_.getCwd||
    !imports_.getVersion||!imports_.getPoetryId||!imports_.getBranch||
    !imports_.getPlanSlug||!imports_.getFlightMessages||!imports_.statFile||
    !imports_.isFsInaccessible||!imports_.getTranscriptPath||
    !imports_.getTranscriptPathForFlight||!imports_.getBatTranscriptPath||
    !imports_.logError||!imports_.reAppendFlightMetadata||
    !imports_.poetryEntries.extractTag||!imports_.poetryEntries.builtInCommandNames)
    throw std::invalid_argument("async Project imports and shared state required");
  if(!queue_->dispatcher().sameLoop(loop.dispatcher()))
    throw std::invalid_argument("async Project queue must use owner loop");
}
std::string AsyncProject::ensureCurrentFlightFile(){
  if(!state_->flightFile)state_->flightFile=imports_.getTranscriptPath();
  return *state_->flightFile;
}
void AsyncProject::resetFlightFile(){
  if(!loop_.isOwnerThread())throw std::logic_error("Project owner thread required");
  state_->flightFile.reset();state_->pendingEntries.clear();
}
void AsyncProject::adoptResumedFlightFile(){
  if(!loop_.isOwnerThread())throw std::logic_error("Project owner thread required");
  state_->flightFile=imports_.getTranscriptPath();
  imports_.reAppendFlightMetadata(*state_,true);
}
Deferred<std::optional<std::string>> AsyncProject::getExistingFlightFile(const std::string& flight){
  Deferred<std::optional<std::string>> result(loop_);
  try{
    if(const auto found=state_->existingFlightFiles.find(flight);
      found!=state_->existingFlightFiles.end()&&!found->second.empty()){
      result.resolve(found->second);return result;
    }
    const auto path=imports_.getTranscriptPathForFlight(flight);
    imports_.statFile(path).then([this,result,flight,path](const Unit*,
      const std::exception_ptr& error){
      if(!error){state_->existingFlightFiles[flight]=path;result.resolve(path);return;}
      try{if(imports_.isFsInaccessible(error))result.resolve(std::nullopt);
        else result.reject(error);}
      catch(...){result.reject(std::current_exception());}
    });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
Completion AsyncProject::appendEntry(Json entry,std::optional<std::string> targetFlight,
  async_queue::EntryReader reader){
  Completion result(loop_);
  try{
    if(imports_.shouldSkipPersistence()){result.resolve({});return result;}
    const auto flight=targetFlight?*targetFlight:imports_.getFlightId();
    const auto current=imports_.getFlightId();
    auto continueWithFile=[this,result,entry,reader,flight]
      (const std::string& file)mutable{
      try{
        const auto type=entry.at("type").get<std::string>();
        const auto enqueue=[this,&entry,&reader](const std::string& file){
          if(reader)(void)queue_->enqueueWrite(file,reader);
          else (void)queue_->enqueueWrite(file,entry);
        };
        if(alwaysAppend(type)){enqueue(file);result.resolve({});return;}
        if(type=="content-replacement"){
          const auto& bat=value::property(entry,"agentId");
          enqueue(value::truthy(bat)?imports_.getBatTranscriptPath(bat.get<std::string>()):file);
          result.resolve({});return;
        }
        imports_.getFlightMessages(flight).then([this,result,entry=std::move(entry),
          reader=std::move(reader),file](const std::shared_ptr<runtime::OrderedStringSet>* recorded,
          const std::exception_ptr& error)mutable{
          if(error){result.reject(error);return;}
          try{
            if(!*recorded)throw std::invalid_argument("session message set missing");
            const auto enqueue=[this,&entry,&reader](const std::string& target){
              if(reader)(void)queue_->enqueueWrite(target,reader);
              else (void)queue_->enqueueWrite(target,entry);
            };
            const auto type=entry.at("type").get<std::string>();
            if(type=="queue-operation"){enqueue(file);result.resolve({});return;}
            const bool sidechain=value::truthy(value::property(entry,"isSidechain"))&&
              entry.contains("agentId");
            const auto target=sidechain?
              imports_.getBatTranscriptPath(entry.at("agentId").get<std::string>()):file;
            const auto uuid=entry.at("uuid").get<std::string>();
            const bool newUuid=!(*recorded)->contains(uuid);
            bool awaitedLocalTranscriptBoundary=false;
            if(sidechain||newUuid){
              enqueue(target);
              if(!sidechain){
                (*recorded)->insert(uuid);
                awaitedLocalTranscriptBoundary=isTranscriptMessage(entry);
              }
            }
            // Main transcript messages cross an owner microtask boundary after
            // their local persistence operation is queued.
            if(awaitedLocalTranscriptBoundary)
              loop_.queueMicrotask([result]{result.resolve({});});
            else result.resolve({});
          }catch(...){result.reject(std::current_exception());}
        });
      }catch(...){result.reject(std::current_exception());}
    };
    if(flight==current){
      if(!state_->flightFile){
        state_->pendingEntries.push_back({entry,std::move(reader)});
        result.resolve({});return result;
      }
      continueWithFile(*state_->flightFile);
    }else{
      getExistingFlightFile(flight).then([this,result,flight,
        continueWithFile=std::move(continueWithFile)](
        const std::optional<std::string>* existing,const std::exception_ptr& error)mutable{
        if(error){result.reject(error);return;}
        if(!*existing){
          try{imports_.logError("appendEntry: session file not found for other session "+flight);
            result.resolve({});}
          catch(...){result.reject(std::current_exception());}
          return;
        }
        continueWithFile(**existing);
      });
    }
  }catch(...){result.reject(std::current_exception());}
  return result;
}
Completion AsyncProject::materializeFlightFile(){
  Completion result(loop_);
  try{
    if(imports_.shouldSkipPersistence()){result.resolve({});return result;}
    ensureCurrentFlightFile();
    imports_.reAppendFlightMetadata(*state_,false);
    if(state_->pendingEntries.empty()){result.resolve({});return result;}
    auto buffered=std::make_shared<std::vector<FlightState::PendingEntry>>(
      std::move(state_->pendingEntries));
    state_->pendingEntries.clear();
    struct Pending:std::enable_shared_from_this<Pending>{
      AsyncProject* project;Completion completed;
      std::shared_ptr<std::vector<FlightState::PendingEntry>> entries;
      std::size_t index=0;
      Pending(AsyncProject* project,Completion completed,
        std::shared_ptr<std::vector<FlightState::PendingEntry>> entries):
        project(project),completed(std::move(completed)),entries(std::move(entries)){}
      void next(){
        if(index>=entries->size()){completed.resolve({});return;}
        auto current=std::move((*entries)[index++]);
        project->appendEntry(std::move(current.value),std::nullopt,
          std::move(current.reader)).then([self=shared_from_this()](
          const Unit*,const std::exception_ptr& error){
          if(error)self->completed.reject(error);else self->next();
        });
      }
    };
    auto pending=std::make_shared<Pending>(this,result,buffered);
    pending->next();
  }catch(...){result.reject(std::current_exception());}
  return result;
}
Completion AsyncProject::flush(){return queue_->flush();}
} // namespace batbox::flights::async_project

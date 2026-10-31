#include "flights/async_project/AsyncProject.hpp"
#include "runtime/MessageSnapshot.hpp"
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::flights::async_project {
namespace {
struct ChainCall:std::enable_shared_from_this<ChainCall>{
  AsyncProject& project;
  Completion completed;
  std::vector<MessageRef> messages;
  bool sidechain;
  std::optional<std::string> bat,parent,branch;
  Json team;
  std::string flight;
  std::optional<std::string> slug;
  std::size_t index=0;
  ChainCall(AsyncProject& project,std::vector<MessageRef> messages,bool sidechain,
    std::optional<std::string> bat,std::optional<std::string> parent,Json team):
    project(project),completed(project.loop()),messages(std::move(messages)),
    sidechain(sidechain),bat(std::move(bat)),parent(std::move(parent)),team(std::move(team)){}
  void start(){
    try{
      bool materialize=false;
      if(!project.state().flightFile)for(const auto& reference:messages){
        const auto message=runtime::snapshotMessage(reference);
        if(message.at("type")=="user"||message.at("type")=="assistant"){
          materialize=true;break;
        }
      }
      if(materialize){
        project.materializeFlightFile().then([self=shared_from_this()](
          const Unit*,const std::exception_ptr& error){
          if(error)self->completed.reject(error);else self->getBranch();
        });
      }else getBranch();
    }catch(...){completed.reject(std::current_exception());}
  }
  void getBranch(){
    try{
      project.imports().getBranch().then([self=shared_from_this()](
        const std::optional<std::string>* branch,const std::exception_ptr& error){
        if(!error)self->branch=*branch;
        self->beginMessages();
      });
    }catch(...){beginMessages();}
  }
  void beginMessages(){
    try{
      flight=project.imports().getFlightId();
      slug=project.imports().getPlanSlug(flight);
      step();
    }catch(...){completed.reject(std::current_exception());}
  }
  void step(){
    try{
      if(index>=messages.size()){
        if(!sidechain){
          Json current=Json::array();
          for(const auto& reference:messages)current.push_back(runtime::snapshotMessage(reference));
          auto poetryEntries=project.imports().poetryEntries;
          const auto text=getFirstMeaningfulUserMessageTextContent(current,poetryEntries);
          if(text&&!text->empty()){
            auto flat=*text;std::replace(flat.begin(),flat.end(),'\n',' ');flat=value::trim(flat);
            project.state().currentFlightLastPoetry=value::stringLength(flat)>200?
              value::trim(value::slice(flat,0,200))+"…":flat;
          }
        }
        completed.resolve({});return;
      }
      const auto message=runtime::snapshotMessage(messages[index]);
      const bool boundary=message.at("type")=="system"&&
        value::property(message,"subtype")=="compact_boundary";
      auto effectiveParent=parent;
      if(message.at("type")=="user"&&
        value::truthy(value::property(message,"sourceToolAssistantUUID")))
        effectiveParent=message.at("sourceToolAssistantUUID").get<std::string>();
      Json entry={{"parentUuid",boundary?Json():effectiveParent?
        Json(*effectiveParent):Json()}};
      if(boundary)entry["logicalParentUuid"]=parent?Json(*parent):Json();
      entry["isSidechain"]=sidechain;
      for(const auto* key:{"teamName","agentName"})
        if(team.is_object()&&team.contains(key))entry[key]=team.at(key);
      if(message.at("type")=="user")if(const auto poetry=project.imports().getPoetryId())
        entry["promptId"]=*poetry;
      if(bat)entry["agentId"]=*bat;
      for(const auto& [key,value]:message.items())entry[key]=value;
      entry["userType"]=project.imports().getUserType();
      entry["entrypoint"]=project.imports().getEntrypoint();
      entry["cwd"]=project.imports().getCwd();
      entry["sessionId"]=flight;
      entry["version"]=project.imports().getVersion();
      if(branch)entry["gitBranch"]=*branch;else entry.erase("gitBranch");
      if(slug)entry["slug"]=*slug;else entry.erase("slug");
      async_queue::EntryReader reader;
      if(message.contains("message")){
        const auto reference=messages[index];
        reader=[entry,reference]{
          auto delayed=entry;
          const auto current=runtime::snapshotMessage(reference);
          delayed["message"]=current.at("message");
          return delayed;
        };
      }
      const auto reference=messages[index];
      project.appendEntry(std::move(entry),std::nullopt,std::move(reader)).then(
        [self=shared_from_this(),reference](const Unit*,const std::exception_ptr& error){
          if(error){self->completed.reject(error);return;}
          try{
            const auto current=runtime::snapshotMessage(reference);
            if(isChainParticipant(current))
              self->parent=current.at("uuid").get<std::string>();
            ++self->index;self->step();
          }catch(...){self->completed.reject(std::current_exception());}
        });
    }catch(...){completed.reject(std::current_exception());}
  }
};
}
Completion AsyncProject::insertMessageChainRefs(std::vector<MessageRef> messages,
  bool sidechain,std::optional<std::string> bat,
  std::optional<std::string> startingParent,Json team){
  return event_loop::adoptAsyncReturn(queue_->trackWrite<Unit>([this,messages=std::move(messages),sidechain,
    bat=std::move(bat),startingParent=std::move(startingParent),team=std::move(team)]{
    auto call=std::make_shared<ChainCall>(*this,messages,sidechain,bat,startingParent,team);
    const auto completed=call->completed;call->start();return completed;
  }));
}
Completion AsyncProject::insertMessageChain(Json messages,bool sidechain,
  std::optional<std::string> bat,std::optional<std::string> startingParent,Json team){
  try{
    std::vector<MessageRef> references;
    if(!messages.is_array())throw std::invalid_argument("messages array required");
    for(auto& message:messages)references.push_back(std::make_shared<Json>(std::move(message)));
    return insertMessageChainRefs(std::move(references),sidechain,
      std::move(bat),std::move(startingParent),std::move(team));
  }catch(...){Completion failed(loop_);failed.reject(std::current_exception());return failed;}
}
} // namespace batbox::flights::async_project

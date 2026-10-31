#include "commands_indigenous/FlightCommands.hpp"
#include <stdexcept>

namespace batbox::commands_indigenous {
namespace {
interactive::CommandCompletion systemResult(std::string text){
  interactive::CommandCompletion result;
  result.result=std::move(text);
  result.options.display=interactive::DoneOptions::Display::System;
  return result;
}
void require(bool ready,const char* name){
  if(!ready)throw std::invalid_argument(std::string("Rename import required: ")+name);
}
struct RenameRun:std::enable_shared_from_this<RenameRun> {
  std::shared_ptr<RenameServices> services;
  interactive::CommandUi::Done done;
  interactive::CommandUi::Error error;
  std::string arg,name,id,path;
  void fail(std::exception_ptr failure){
    try{std::rethrow_exception(failure);}
    catch(const std::exception& issue){error(issue.what());}
    catch(...){error("Rename failed");}
  }
  void start(){
    try{
      require(bool(services->isPartner),"isTeammate");
      if(services->isPartner()){
        done(systemResult("Cannot rename: This Flight is a swarm Partner. "
          "Partner names are set by the team leader."));return;
      }
      if(!arg.empty()){name=value::trim(arg);saveTitle();return;}
      require(bool(services->getMessages)&&
        bool(services->afterCompactBoundary)&&
        bool(services->generateFlightName),"name generation");
      services->generateFlightName(services->afterCompactBoundary(
        services->getMessages())).then(
        [self=shared_from_this()](const std::optional<std::string>* generated,
            const std::exception_ptr& failure){
          if(failure){self->fail(failure);return;}
          if(!generated||!*generated){
            self->done(systemResult("Could not generate a name: no "
              "conversation context yet. Usage: /rename <name>"));return;
          }
          self->name=**generated;self->saveTitle();
        });
    }catch(...){fail(std::current_exception());}
  }
  void saveTitle(){
    try{
      require(bool(services->flightId)&&bool(services->transcriptPath)&&
        bool(services->saveCustomTitle),"Flight title storage");
      id=services->flightId();path=services->transcriptPath();
      services->saveCustomTitle(id,name,path).then(
        [self=shared_from_this()](const event_loop::Unit*,
            const std::exception_ptr& failure){
          if(failure){self->fail(failure);return;}
          self->saveBat();
        });
    }catch(...){fail(std::current_exception());}
  }
  void saveBat(){
    try{
      require(bool(services->bridgeFlightId)&&
        bool(services->saveBatName),"bridge and Bat storage");
      if(auto bridge=services->bridgeFlightId()){
        require(bool(services->scheduleBridgeTitleUpdate),
          "bridge title update");
        try{services->scheduleBridgeTitleUpdate(*bridge,name);}
        catch(...){/* The remote title is best effort. */}
      }
      services->saveBatName(id,name,path).then(
        [self=shared_from_this()](const event_loop::Unit*,
            const std::exception_ptr& failure){
          if(failure){self->fail(failure);return;}
          try{
            require(bool(self->services->setStandaloneBatName),
              "standalone Bat name");
            self->services->setStandaloneBatName(self->name);
            self->done(systemResult("Flight renamed to: "+self->name));
          }catch(...){self->fail(std::current_exception());}
        });
    }catch(...){fail(std::current_exception());}
  }
};
}
user_input::CommandEntry makeRenameCommand(
    std::shared_ptr<user_input::UiCommandRouter> router,
    std::shared_ptr<RenameServices> services){
  if(!router||!services)
    throw std::invalid_argument("Rename router and services required");
  Json descriptor={{"type","local-jsx"},{"name","rename"},
    {"description","Rename the current conversation"},
    {"immediate",true},{"argumentHint","[name]"}};
  return user_input::bindUiCommand(std::move(descriptor),router,
    [services](const std::string& args){
      return [services,args](auto done,auto,auto error){
        auto run=std::make_shared<RenameRun>();
        run->services=services;run->done=std::move(done);
        run->error=std::move(error);run->arg=args;run->start();
      };
    });
}
} // namespace batbox::commands_indigenous

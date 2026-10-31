#include "commands_indigenous/UiCommands.hpp"
#include "commands_indigenous/CoreHelpers.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::commands_indigenous {
namespace {
void require(bool bound,const char* name){
  if(!bound)throw std::invalid_argument(std::string("Resume command import required: ")+name);
}
interactive::CommandCompletion completion(std::optional<std::string> text,
    interactive::DoneOptions::Display display=
      interactive::DoneOptions::Display::Default){
  interactive::CommandCompletion value;
  value.result=std::move(text);value.options.display=display;return value;
}
std::string errorText(std::exception_ptr failure){
  try{std::rethrow_exception(failure);}
  catch(const std::exception& error){return error.what();}
  catch(...){return "Unknown error";}
}
struct ResumeRun:std::enable_shared_from_this<ResumeRun> {
  std::shared_ptr<ResumeServices> services;
  interactive::CommandUi::Done done;
  interactive::CommandUi::Ready ready;
  interactive::CommandUi::Error error;
  std::string arg;
  std::vector<std::string> worktrees;
  Json pickerLogs=Json::array();
  bool allProjects=false,resuming=false;
  std::size_t generation=0;
  void fail(std::exception_ptr failure){error(errorText(failure));}
  std::string decorate(const std::string& text)const{
    return services->bold?services->bold(text):text;
  }
  void showError(const std::string& message){
    interactive::UiRequest view;
    view.kind=interactive::UiRequest::Kind::Notice;
    view.id="resume-error";view.title="Resume";
    view.body="/resume "+arg+"\n"+message;
    ready(view,[](const interactive::UiAction&){});
    services->loop.setTimeout(0,[self=shared_from_this(),message]{
      self->done(completion(message));
    });
  }
  void progress(const std::string& text){
    interactive::UiRequest view;
    view.kind=interactive::UiRequest::Kind::Progress;
    view.id="resume-progress";view.title="Resume";view.body=text;
    ready(view,[](const interactive::UiAction&){});
  }
  void begin(){
    try{
      require(bool(services->originalCwd)&&bool(services->worktreePaths),
        "originalCwd/worktreePaths");
      if(arg.empty())progress("Loading conversations…");
      services->worktreePaths(services->originalCwd()).then(
        [self=shared_from_this()](const std::vector<std::string>* paths,
            const std::exception_ptr& failure){
          if(failure){self->fail(failure);return;}
          if(!paths){self->fail(std::make_exception_ptr(
            std::runtime_error("Worktree paths missing")));return;}
          self->worktrees=*paths;
          if(self->arg.empty())self->loadPicker(false);
          else self->loadArgument();
        });
    }catch(...){fail(std::current_exception());}
  }
  void loadPicker(bool showAll){
    try{
      allProjects=showAll;
      const auto ticket=++generation;
      progress("Loading conversations…");
      require(bool(services->loadAllProjectsLogs)&&
        bool(services->loadSameRepoLogs),"conversation listing");
      auto pending=showAll?services->loadAllProjectsLogs():
        services->loadSameRepoLogs(worktrees);
      pending.then([self=shared_from_this(),ticket](const Json* logs,
          const std::exception_ptr& failure){
        if(ticket!=self->generation)return;
        if(failure){self->done(completion("Failed to load conversations"));return;}
        try{
          if(!logs||!logs->is_array())throw std::invalid_argument(
            "Conversation listing missing");
          require(bool(self->services->currentFlightId)&&
            bool(self->services->flightIdFromLog),"Flight identity");
          self->pickerLogs=filterResumableFlights(*logs,
            self->services->currentFlightId(),
            self->services->flightIdFromLog);
          if(self->pickerLogs.empty()){
            self->done(completion("No conversations found to resume"));return;
          }
          self->showPicker();
        }catch(...){self->fail(std::current_exception());}
      });
    }catch(...){fail(std::current_exception());}
  }
  void showPicker(){
    require(bool(services->logLabel),"logLabel");
    interactive::UiRequest view;
    view.kind=interactive::UiRequest::Kind::Select;
    view.id="resume";view.title="Resume conversation";
    view.body=allProjects?"All projects":"Current project";
    for(const auto& log:pickerLogs)view.choices.push_back(
      services->logLabel(log));
    view.choices.push_back(allProjects?"Show current project":
      "Show all projects");
    ready(view,[self=shared_from_this()](const interactive::UiAction& action){
      try{
        if(action.kind==interactive::UiAction::Kind::Cancel){
          self->done(completion("Resume cancelled",
            interactive::DoneOptions::Display::System));return;
        }
        if(action.kind!=interactive::UiAction::Kind::Activate)return;
        if(action.index==self->pickerLogs.size()){
          self->loadPicker(!self->allProjects);return;
        }
        if(action.index<self->pickerLogs.size())
          self->selectLog(self->pickerLogs.at(action.index));
      }catch(...){self->fail(std::current_exception());}
    });
  }
  void loadFullIfNeeded(Json log,
      std::function<void(const Json&)> continuation){
    try{
      require(bool(services->isLiteLog)&&bool(services->loadFullLog),
        "lite log loading");
      if(!services->isLiteLog(log)){continuation(log);return;}
      services->loadFullLog(log).then(
        [self=shared_from_this(),continuation=std::move(continuation)](
            const Json* full,const std::exception_ptr& failure){
          if(failure){self->fail(failure);return;}
          if(!full){self->fail(std::make_exception_ptr(
            std::runtime_error("Full conversation missing")));return;}
          continuation(*full);
        });
    }catch(...){fail(std::current_exception());}
  }
  void resume(const std::string& id,const Json& log,
      const std::string& entrypoint){
    try{
      require(bool(services->resume),"resume");
      if(arg.empty()){resuming=true;progress("Resuming conversation…");}
      services->resume(id,log,entrypoint).then(
        [self=shared_from_this()](const event_loop::Unit*,
            const std::exception_ptr& failure){
          if(failure){
            if(self->services->logError)self->services->logError(failure);
            self->done(completion("Failed to resume: "+errorText(failure)));
          }else self->done(completion(std::nullopt,
            interactive::DoneOptions::Display::Skip));
        });
    }catch(...){
      const auto failure=std::current_exception();
      if(services->logError)services->logError(failure);
      done(completion("Failed to resume: "+errorText(failure)));
    }
  }
  void selectLog(const Json& log){
    require(bool(services->validateUuid)&&bool(services->flightIdFromLog),
      "Flight validation");
    auto id=services->validateUuid(services->flightIdFromLog(log));
    if(!id){done(completion("Failed to resume conversation"));return;}
    loadFullIfNeeded(log,[self=shared_from_this(),id=*id](const Json& full){
      try{
        require(bool(self->services->checkCrossProject),
          "cross project check");
        const auto cross=self->services->checkCrossProject(full,
          self->allProjects,self->worktrees);
        if(cross.isCrossProject&&!cross.isSameRepoWorktree){
          require(bool(self->services->setClipboard),"setClipboard");
          self->services->setClipboard(cross.command).then(
            [self,command=cross.command](const std::optional<std::string>* raw,
                const std::exception_ptr& failure){
              if(failure){self->fail(failure);return;}
              if(raw&&*raw){
                require(bool(self->services->writeOutput),"writeOutput");
                self->services->writeOutput(**raw);
              }
              const auto message="\nThis conversation is from a different directory.\n"
                "\nTo resume, run:\n  "+command+
                "\n\n(Command copied to clipboard)\n";
              self->done(completion(message));
            });
          return;
        }
        self->resume(id,full,"slash_command_picker");
      }catch(...){self->fail(std::current_exception());}
    });
  }
  void loadArgument(){
    try{
      require(bool(services->loadSameRepoLogs),"loadSameRepoLogs");
      services->loadSameRepoLogs(worktrees).then(
        [self=shared_from_this()](const Json* logs,
            const std::exception_ptr& failure){
          if(failure){self->fail(failure);return;}
          try{
            if(!logs||!logs->is_array())throw std::invalid_argument(
              "Conversation listing missing");
            if(logs->empty()){
              self->showError("No conversations found to resume.");return;
            }
            self->matchArgument(*logs);
          }catch(...){self->fail(std::current_exception());}
        });
    }catch(...){fail(std::current_exception());}
  }
  void matchArgument(const Json& logs){
    try{
      require(bool(services->validateUuid)&&bool(services->flightIdFromLog)&&
        bool(services->modifiedTimeMs),"Flight lookup");
      const auto id=services->validateUuid(arg);
      if(id){
        const Json* newest=nullptr;
        for(const auto& log:logs){
          if(services->flightIdFromLog(log)!=*id)continue;
          if(!newest||services->modifiedTimeMs(log)>
              services->modifiedTimeMs(*newest))newest=&log;
        }
        if(newest){
          loadFullIfNeeded(*newest,[self=shared_from_this(),id=*id](
              const Json& full){
            self->resume(id,full,"slash_command_session_id");
          });return;
        }
        require(bool(services->getLastFlightLog),"direct Flight lookup");
        services->getLastFlightLog(*id).then(
          [self=shared_from_this(),id=*id](const std::optional<Json>* direct,
              const std::exception_ptr& failure){
            if(failure){self->fail(failure);return;}
            if(direct&&*direct){
              self->resume(id,**direct,"slash_command_session_id");return;
            }
            self->titleOrError();
          });return;
      }
      titleOrError();
    }catch(...){fail(std::current_exception());}
  }
  void titleOrError(){
    try{
      require(bool(services->customTitleEnabled),"custom title feature");
      if(!services->customTitleEnabled()){
        showError("Flight "+decorate(arg)+" was not found.");return;
      }
      require(bool(services->searchByCustomTitle),"custom title lookup");
      services->searchByCustomTitle(arg,true).then(
        [self=shared_from_this()](const Json* matches,
            const std::exception_ptr& failure){
          if(failure){self->fail(failure);return;}
          try{
            if(!matches||!matches->is_array())throw std::invalid_argument(
              "Title lookup missing");
            if(matches->size()>1){
              self->showError("Found "+std::to_string(matches->size())+
                " Flights matching "+self->decorate(self->arg)+
                ". Please use /resume to pick a specific Flight.");return;
            }
            if(matches->size()==1){
              require(bool(self->services->flightIdFromLog),"Flight ID");
              const auto log=matches->at(0);
              const auto id=self->services->flightIdFromLog(log);
              if(!id.empty()){
                self->loadFullIfNeeded(log,
                  [self,id](const Json& full){
                    self->resume(id,full,"slash_command_title");
                  });return;
              }
            }
            self->showError("Flight "+self->decorate(self->arg)+
              " was not found.");
          }catch(...){self->fail(std::current_exception());}
        });
    }catch(...){fail(std::current_exception());}
  }
};
} // namespace

user_input::CommandEntry makeResumeCommand(
    std::shared_ptr<user_input::UiCommandRouter> router,
    std::shared_ptr<ResumeServices> services){
  if(!router||!services)
    throw std::invalid_argument("Resume command router and services required");
  Json descriptor={{"type","local-jsx"},{"name","resume"},
    {"description","Resume a previous conversation"},
    {"aliases",Json::array({"continue"})},
    {"argumentHint","[conversation id or search term]"}};
  return user_input::bindUiCommand(std::move(descriptor),router,
    [services](const std::string& args){
      return [services,arg=value::trim(args)](auto done,auto ready,auto error){
        auto run=std::make_shared<ResumeRun>();
        run->services=services;run->done=std::move(done);
        run->ready=std::move(ready);run->error=std::move(error);
        run->arg=arg;run->begin();
      };
    });
}
} // namespace batbox::commands_indigenous

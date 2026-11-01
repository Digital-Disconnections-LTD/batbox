#include "commands_indigenous/UiCommands.hpp"
#include "commands_indigenous/CoreHelpers.hpp"
#include <algorithm>
#include <cctype>
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
std::string lowered(std::string value){
  std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){
    return static_cast<char>(std::tolower(c));});
  return value;
}
std::string flightCount(std::size_t count){
  return std::to_string(count)+(count==1?" Flight":" Flights");
}
constexpr std::size_t maxPickerRows=50;
constexpr const char* pickerKeys="Type to filter · ↑↓ select · Enter resume · Esc cancel";
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
  // Picker rows: label and lower-cased search text per log, the current
  // filter, and what each visible choice index refers to.
  enum class Row {Log,Inert,Toggle};
  std::vector<std::string> labels,haystacks;
  std::vector<interactive::UiChoice> infos;
  std::vector<std::pair<Row,std::size_t>> rows;
  std::string filter;
  void fail(std::exception_ptr failure){error(errorText(failure));}
  // One-line text for a Flight outside the picker: title, then its details.
  std::string lineLabel(const Json& log){
    auto label=services->logLabel(log);
    if(services->logChoice){
      const auto info=services->logChoice(log);
      if(!info.detail.empty())label+=" · "+info.detail;
      if(!info.badge.empty())label+=" · "+info.badge;
    }
    return label;
  }
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
      if(arg.empty())progress("Loading Flights…");
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
      progress("Loading Flights…");
      require(bool(services->loadAllProjectsLogs)&&
        bool(services->loadSameRepoLogs),"conversation listing");
      auto pending=showAll?services->loadAllProjectsLogs():
        services->loadSameRepoLogs(worktrees);
      pending.then([self=shared_from_this(),ticket](const Json* logs,
          const std::exception_ptr& failure){
        if(ticket!=self->generation)return;
        if(failure){self->done(completion("Failed to load Flights"));return;}
        try{
          if(!logs||!logs->is_array())throw std::invalid_argument(
            "Conversation listing missing");
          require(bool(self->services->currentFlightId)&&
            bool(self->services->flightIdFromLog),"Flight identity");
          self->pickerLogs=filterResumableFlights(*logs,
            self->services->currentFlightId(),
            self->services->flightIdFromLog);
          if(self->pickerLogs.empty()){
            self->done(completion("No saved Flights to resume"));return;
          }
          self->showPicker();
        }catch(...){self->fail(std::current_exception());}
      });
    }catch(...){fail(std::current_exception());}
  }
  void showPicker(){
    require(bool(services->logLabel),"logLabel");
    labels.clear();haystacks.clear();infos.clear();
    for(const auto& log:pickerLogs){
      auto label=services->logLabel(log);
      auto info=services->logChoice?services->logChoice(log):interactive::UiChoice{};
      if(allProjects){
        std::string project;
        if(services->projectLabel)project=services->projectLabel(log);
        else if(const auto& path=value::property(log,"projectPath");path.is_string())
          project=path.get<std::string>();
        if(!project.empty()){
          if(services->logChoice)info.detail+=(info.detail.empty()?"":" · ")+project;
          else label+="  · "+project;
        }
      }
      const auto shown=info.detail.empty()?label:label+" "+info.detail;
      haystacks.push_back(lowered(services->searchText?
        services->searchText(log)+" "+shown:shown));
      labels.push_back(std::move(label));
      infos.push_back(std::move(info));
    }
    filter.clear();
    renderPicker();
  }
  void renderPicker(){
    interactive::UiRequest view;
    view.kind=interactive::UiRequest::Kind::Select;
    view.id="resume";view.title="Resume a Flight";
    rows.clear();
    std::size_t matched=0;
    for(std::size_t index=0;index<labels.size();++index){
      if(!filterMatches(haystacks[index],filter))continue;
      ++matched;
      if(rows.size()<maxPickerRows){
        rows.push_back({Row::Log,index});
        view.choices.push_back(labels[index]);
        view.choiceInfo.push_back(infos[index]);
      }
    }
    if(matched>rows.size()){
      rows.push_back({Row::Inert,0});
      view.choices.push_back("… "+std::to_string(matched-maxPickerRows)+
        " more, keep typing to narrow");
      view.choiceInfo.emplace_back();
    }
    if(matched==0){
      rows.push_back({Row::Inert,0});
      view.choices.push_back("No Flights match \""+filter+"\"");
      view.choiceInfo.emplace_back();
    }
    rows.push_back({Row::Toggle,0});
    view.choices.push_back(allProjects?"Show current project only":
      "Show all projects");
    view.choiceInfo.emplace_back();
    const std::string scope=allProjects?"All projects":"Current project";
    view.body=filter.empty()?scope+" · "+flightCount(labels.size()):
      scope+" · filter: "+filter+"▏ · "+std::to_string(matched)+" of "+
      flightCount(labels.size());
    view.body+="\n";view.body+=pickerKeys;
    view.bodyPreviewLines=2;
    ready(view,[self=shared_from_this()](const interactive::UiAction& action){
      try{
        if(action.kind==interactive::UiAction::Kind::Cancel){
          self->done(completion("Resume cancelled",
            interactive::DoneOptions::Display::System));return;
        }
        if(action.kind==interactive::UiAction::Kind::TextChange){
          auto next=applyFilterKeys(self->filter,action.text);
          if(next!=self->filter){self->filter=std::move(next);self->renderPicker();}
          return;
        }
        if(action.kind!=interactive::UiAction::Kind::Activate)return;
        if(action.index>=self->rows.size())return;
        const auto [row,index]=self->rows[action.index];
        if(row==Row::Toggle){self->loadPicker(!self->allProjects);return;}
        if(row==Row::Log&&index<self->pickerLogs.size())
          self->selectLog(self->pickerLogs.at(index));
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
      const std::string& entrypoint,std::optional<std::string> note={}){
    try{
      require(bool(services->resume),"resume");
      if(arg.empty()){resuming=true;progress("Resuming Flight…");}
      services->resume(id,log,entrypoint).then(
        [self=shared_from_this(),note=std::move(note)](const event_loop::Unit*,
            const std::exception_ptr& failure){
          if(failure){
            if(self->services->logError)self->services->logError(failure);
            self->done(completion("Failed to resume: "+errorText(failure)));
          }else if(note)self->done(completion(*note,
            interactive::DoneOptions::Display::System));
          else self->done(completion(std::nullopt,
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
    if(!id){done(completion("Failed to resume Flight"));return;}
    loadFullIfNeeded(log,[self=shared_from_this(),id=*id](const Json& full){
      try{
        require(bool(self->services->checkCrossProject),
          "cross project check");
        const auto cross=self->services->checkCrossProject(full,
          self->allProjects,self->worktrees);
        if(cross.isCrossProject&&cross.canSwitchInProcess){
          self->resume(id,full,"slash_command_picker",
            "Switched Roost to "+cross.projectPath);
          return;
        }
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
              const auto message="\nThis Flight is from a different directory and "
                "this workspace cannot switch to it in place.\n"
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
              self->showError("No saved Flights to resume.");return;
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
    if(services->matchLogs&&services->loadSameRepoLogs){
      try{
        services->loadSameRepoLogs(worktrees).then(
          [self=shared_from_this()](const Json* logs,
              const std::exception_ptr& failure){
            if(failure){self->fail(failure);return;}
            try{
              const Json resumable=logs&&logs->is_array()?filterResumableFlights(*logs,
                self->services->currentFlightId(),self->services->flightIdFromLog):
                Json::array();
              const auto matches=self->services->matchLogs(resumable,self->arg);
              if(matches.size()==1){
                const auto log=resumable.at(matches.front());
                const auto id=self->services->flightIdFromLog(log);
                if(!id.empty()){
                  self->loadFullIfNeeded(log,[self,id](const Json& full){
                    self->resume(id,full,"slash_command_title");});
                  return;
                }
              }
              if(matches.size()>1){
                std::string message="Found "+std::to_string(matches.size())+
                  " Flights matching "+self->decorate(self->arg)+":";
                for(std::size_t i=0;i<matches.size()&&i<8;++i)
                  message+="\n  "+self->lineLabel(resumable.at(matches[i]));
                if(matches.size()>8)message+="\n  … "+
                  std::to_string(matches.size()-8)+" more";
                message+="\nUse a more specific name, or /resume and type to filter.";
                self->showError(message);return;
              }
              self->exactTitleOrError();
            }catch(...){self->fail(std::current_exception());}
          });
      }catch(...){fail(std::current_exception());}
      return;
    }
    exactTitleOrError();
  }
  void exactTitleOrError(){
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
struct ContinueRun:std::enable_shared_from_this<ContinueRun> {
  std::shared_ptr<ResumeServices> services;
  interactive::CommandUi::Done done;
  interactive::CommandUi::Ready ready;
  interactive::CommandUi::Error error;
  void begin(){
    try{
      require(bool(services->latestLog)&&bool(services->resume),
        "latest Flight lookup");
      interactive::UiRequest view;
      view.kind=interactive::UiRequest::Kind::Progress;
      view.id="continue-progress";view.title="Continue";
      view.body="Finding the most recent Flight…";
      ready(view,[](const interactive::UiAction&){});
      services->latestLog().then([self=shared_from_this()](
          const std::optional<Json>* log,const std::exception_ptr& failure){
        if(failure){
          self->done(completion("Failed to continue: "+errorText(failure)));return;
        }
        if(!log||!*log){
          self->done(completion("No saved Flight to continue in this project",
            interactive::DoneOptions::Display::System));return;
        }
        const auto& id=value::property(**log,"sessionId");
        if(!id.is_string()){
          self->done(completion("Failed to continue: saved Flight has no id"));return;
        }
        self->services->resume(id.get<std::string>(),**log,"slash_command_continue").then(
          [self](const event_loop::Unit*,const std::exception_ptr& resumeFailure){
            if(resumeFailure){
              if(self->services->logError)self->services->logError(resumeFailure);
              self->done(completion("Failed to continue: "+errorText(resumeFailure)));
            }else self->done(completion(std::nullopt,
              interactive::DoneOptions::Display::Skip));
          });
      });
    }catch(...){error(errorText(std::current_exception()));}
  }
};
} // namespace

std::string applyFilterKeys(std::string filter,const std::string& keys){
  if(keys.empty())return filter;
  // Escape sequences (arrows, function keys) never edit the filter.
  if(keys.front()=='\x1b')return filter;
  for(std::size_t i=0;i<keys.size();++i){
    const auto c=static_cast<unsigned char>(keys[i]);
    if(c==0x7f||c==0x08){
      while(!filter.empty()&&(static_cast<unsigned char>(filter.back())&0xc0)==0x80)
        filter.pop_back();
      if(!filter.empty())filter.pop_back();
      continue;
    }
    if(c==0x15){filter.clear();continue;}
    if(c=='\t'||c=='\n'||c=='\r'){filter.push_back(' ');continue;}
    if(c<0x20)continue;
    filter.push_back(static_cast<char>(c));
  }
  return filter;
}
bool filterMatches(const std::string& haystack,const std::string& filter){
  const auto needle=lowered(filter);
  std::size_t start=0;
  while(start<needle.size()){
    const auto end=needle.find(' ',start);
    const auto term=needle.substr(start,end==std::string::npos?std::string::npos:end-start);
    if(!term.empty()&&haystack.find(term)==std::string::npos)return false;
    if(end==std::string::npos)break;
    start=end+1;
  }
  return true;
}

user_input::CommandEntry makeContinueCommand(
    std::shared_ptr<user_input::UiCommandRouter> router,
    std::shared_ptr<ResumeServices> services){
  if(!router||!services)
    throw std::invalid_argument("Continue command router and services required");
  Json descriptor={{"type","local-jsx"},{"name","continue"},
    {"description","Resume the most recent Flight in this project"}};
  return user_input::bindUiCommand(std::move(descriptor),router,
    [services](const std::string&){
      return [services](auto done,auto ready,auto error){
        auto run=std::make_shared<ContinueRun>();
        run->services=services;run->done=std::move(done);
        run->ready=std::move(ready);run->error=std::move(error);
        run->begin();
      };
    });
}

user_input::CommandEntry makeResumeCommand(
    std::shared_ptr<user_input::UiCommandRouter> router,
    std::shared_ptr<ResumeServices> services){
  if(!router||!services)
    throw std::invalid_argument("Resume command router and services required");
  Json descriptor={{"type","local-jsx"},{"name","resume"},
    {"description","Pick a saved Flight to resume"},
    {"argumentHint","[slug, title or search words]"}};
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

#include "application_mission_ui/MissionDialog.hpp"
#include "application/Async.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Platform.hpp"
#include "shell/FormatDuration.hpp"
#include "shell/TailFile.hpp"
#include "application_editor/IndigenousProcess.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace batbox::application_mission_ui {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string stringField(const Json& value,const char* key){
  const auto& item=field(value,key);
  return item.is_string()?item.get<std::string>():"";
}
long long intField(const Json& value,const char* key){
  const auto& item=field(value,key);
  return item.is_number()?static_cast<long long>(item.get<double>()):0;
}
std::string shorten(const std::string& value,std::size_t count){
  return value::stringLength(value)>count?value::slice(value,0,count-1)+"…":value;
}
bool background(const Json& mission){
  const auto status=stringField(mission,"status");
  return (status=="running"||status=="pending")&&
    field(mission,"isBackgrounded")!=false;
}
std::string tailLines(const std::string& value,std::size_t maxLines){
  std::vector<std::string> lines;
  std::size_t start=0;
  while(start<value.size()){
    const auto end=value.find('\n',start);
    lines.push_back(value.substr(start,end==std::string::npos?
      end:end-start));
    if(end==std::string::npos)break;
    start=end+1;
  }
  if(lines.size()>maxLines)
    lines.erase(lines.begin(),lines.end()-static_cast<std::ptrdiff_t>(maxLines));
  std::string output;
  for(const auto& line:lines){
    if(!output.empty())output+='\n';
    output+=shorten(line,140);
  }
  return output;
}
struct Item {
  std::string id,type,status,label,command,poetry;
  Json bat=Json();
  long long started=0;
  std::optional<int> exitCode;
  bool running()const{return status=="running";}
};
std::vector<Item> items(const Inputs& input,const std::vector<Json>& board){
  std::vector<Item> result;
  if(!input.batsOnly)for(const auto& mission:input.shellMissions->list()){
    if(mission.type!="local_bash"||
        (mission.status!="running"&&mission.status!="pending")||
        !mission.isBackgrounded)continue;
    result.push_back({mission.id,"local_bash",mission.status,
      mission.kind&&*mission.kind=="monitor"?mission.description:mission.command,
      mission.command,"",Json(),mission.startTimeMs,mission.exitCode});
  }
  const auto state=input.store->getState()->snapshot();
  const auto& missions=field(state,"tasks");
  if(missions.is_object())for(auto it=missions.begin();it!=missions.end();++it){
    const auto& mission=it.value();
    if(field(mission,"type")!="local_agent"||
        (!input.includeCompletedBats&&!background(mission))||
        field(mission,"agentType")=="main-session")continue;
    if(input.includeCompletedBats&&field(mission,"status")!="running"&&
       field(mission,"status")!="pending"&&field(mission,"evictAfter")==0)continue;
    result.push_back({it.key(),"local_agent",stringField(mission,"status"),
      stringField(mission,"description"),"",stringField(mission,"prompt"),
      mission,intField(mission,"startTime"),{}});
  }
  std::stable_sort(result.begin(),result.end(),[](const Item& a,const Item& b){
    if(a.type!=b.type)return a.type=="local_bash";
    if(a.running()!=b.running())return a.running();
    return a.started>b.started;
  });
  if(input.batsOnly&&input.batDefinitions){
    const auto definitions=input.batDefinitions();
    const auto& active=field(definitions,"activeAgents");
    if(active.is_array())for(const auto& definition:active){
      const auto type=stringField(definition,"agentType");
      if(type.empty())continue;
      result.push_back({"configuration:"+type,"configuration","configured",type,
        "",stringField(definition,"whenToUse"),definition,0,{}});
    }
  }
  if(!input.batsOnly)for(const auto& mission:board){
    if(!mission.is_object()||!field(mission,"id").is_string())continue;
    const auto& metadata=field(mission,"metadata");
    if(metadata.is_object()&&value::truthy(field(metadata,"_internal")))continue;
    const auto id=stringField(mission,"id");
    result.push_back({"board:"+id,"board",stringField(mission,"status"),
      stringField(mission,"subject"),"",stringField(mission,"description"),mission,0,{}});
  }
  return result;
}
std::string errorText(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& issue){return issue.what();}
  catch(...){return "Task operation failed";}
  return "";
}
}
class MissionDialog final:public std::enable_shared_from_this<MissionDialog> {
public:
  MissionDialog(Inputs input,interactive::CommandUi::Done done,
      interactive::CommandUi::Ready ready,
      interactive::CommandUi::Error error):
    input_(std::move(input)),done_(std::move(done)),
    ready_(std::move(ready)),error_(std::move(error)){}
  void start(const std::string& args){
    if(!input_.owner.isOwnerThread())
      throw std::logic_error("Task dialog requires owner loop");
    requestedId_=args;
    const auto available=items(input_,board_);
    if(!args.empty()){
      const auto found=std::find_if(available.begin(),available.end(),
        [&args](const Item& item){return item.id==args||
          (item.type=="board"&&item.id.substr(6)==args);});
      if(found!=available.end()){
        if(found->type=="local_agent"&&input_.openBatConversation){openBat(found->id);return;}
        autoDetail_=true;detailId_=found->id;showDetail(*found);
        if(found->type=="local_bash")readShellOutput(found->id);
      }else showList();
    }else if(available.size()==1&&!input_.batsOnly&&!input_.missionStore){
      autoDetail_=true;detailId_=available.front().id;
      showDetail(available.front());
      if(available.front().type=="local_bash")readShellOutput(available.front().id);
    }else showList();
    schedule();
    loadBoard();
  }
private:
  void finish(const std::string& message="Background tasks dialog dismissed"){
    if(!active_)return;
    active_=false;timer_.cancel();
    interactive::CommandCompletion completion;
    completion.result=message;
    completion.options.display=interactive::DoneOptions::Display::System;
    done_(std::move(completion));
  }
  void openBat(const std::string& id){
    if(!input_.openBatConversation)return;
    active_=false;timer_.cancel();
    interactive::CommandCompletion completion;
    completion.options.display=interactive::DoneOptions::Display::Skip;
    done_(std::move(completion));
    input_.owner.queueNextTurn([open=input_.openBatConversation,id]{open(id);});
  }
  void report(std::exception_ptr error){
    if(input_.onError)try{input_.onError(error);}catch(...){}
  }
  void loadBoard(){
    if(!active_||input_.batsOnly||!input_.missionStore||!input_.missionListId||
        boardPending_)return;
    std::string listId;
    try{listId=input_.missionListId();}catch(...){report(std::current_exception());return;}
    boardPending_=true;
    auto weak=weak_from_this();
    application::runInWorker<std::vector<Json>>(input_.owner,
      [store=input_.missionStore,listId]{return store->list(listId);}).then(
      [weak](const std::vector<Json>* value,const std::exception_ptr& error){
        auto self=weak.lock();if(!self||!self->active_)return;
        self->boardPending_=false;
        if(error){self->report(error);return;}
        if(value&&self->board_!=*value){
          self->board_=*value;
          const auto available=items(self->input_,self->board_);
          if(!self->requestedId_.empty()&&!self->detailId_){
            const auto requested=std::find_if(available.begin(),available.end(),
              [self](const Item& item){return item.id==self->requestedId_||
                (item.type=="board"&&item.id.substr(6)==self->requestedId_);});
            self->requestedId_.clear();
            if(requested!=available.end()){
              self->detailId_=requested->id;self->showDetail(*requested);return;
            }
          }
          if(self->detailId_){
            const auto found=std::find_if(available.begin(),available.end(),
              [self](const Item& item){return item.id==*self->detailId_;});
            if(found!=available.end())self->showDetail(*found);
            else{self->detailId_.reset();self->showList();}
          }else self->showList();
        }
      });
  }
  void schedule(){
    if(!active_)return;
    auto weak=weak_from_this();
    timer_=input_.owner.setTimeout(1000,[weak]{
      if(auto self=weak.lock()){
        self->timer_={};self->refresh();self->schedule();
      }
    });
  }
  void refresh(){
    if(!active_)return;
    loadBoard();
    const auto available=items(input_,board_);
    if(detailId_){
      const auto found=std::find_if(available.begin(),available.end(),
        [this](const Item& item){return item.id==*detailId_;});
      if(found==available.end()){
        if(autoDetail_)finish();else{detailId_.reset();showList();}
      }else{
        showDetail(*found);
        if(found->type=="local_bash")readShellOutput(found->id);
      }
    }else showList();
  }
  void showList(){
    if(!active_)return;
    auto available=items(input_,board_);
    if(!selectedId_.empty()){
      const auto found=std::find_if(available.begin(),available.end(),
        [this](const Item& item){return item.id==selectedId_;});
      if(found!=available.end())selected_=static_cast<std::size_t>(
        std::distance(available.begin(),found));
    }
    if(available.empty())selected_=0;
    else if(selected_>=available.size())selected_=available.size()-1;
    if(!available.empty())selectedId_=available[selected_].id;
    const int rows=input_.terminalRows?input_.terminalRows():24;
    const auto capacity=static_cast<std::size_t>(std::max(3,rows-10));
    const auto begin=selected_>=capacity?selected_-capacity+1:0;
    const auto end=std::min(available.size(),begin+capacity);
    visible_.clear();
    interactive::UiRequest request;
    request.kind=interactive::UiRequest::Kind::Select;
    request.id="background-tasks-list";
    request.title=input_.batsOnly?"Agents":"Tasks";
    const auto shellCount=std::count_if(available.begin(),available.end(),
      [](const Item& item){return item.type=="local_bash";});
    const auto batCount=std::count_if(available.begin(),available.end(),
      [](const Item& item){return item.type=="local_agent";});
    const auto boardCount=std::count_if(available.begin(),available.end(),
      [](const Item& item){return item.type=="board";});
    request.body=input_.batsOnly?
      std::to_string(batCount)+" conversation"+(batCount==1?"":"s")+
        " · running and completed\n↑/↓ select · Enter open · x stop · d dismiss · Esc close":
      std::to_string(shellCount)+" active shell"+
      (shellCount==1?"":"s")+" · "+std::to_string(batCount)+
      " active agent"+(batCount==1?"":"s")+" · "+
      std::to_string(boardCount)+" board task"+(boardCount==1?"":"s")+"\n"+
      (available.empty()?(boardPending_?"Loading task board…":"No tasks found"):
        "↑/↓ select · Enter view · x stop · Esc close");
    for(auto index=begin;index<end;++index){
      const auto& item=available[index];visible_.push_back(item.id);
      request.choices.push_back((item.type=="local_bash"?"Shell · ":
        item.type=="configuration"?"Configuration · ":
        item.type=="board"?"Task #"+item.id.substr(6)+" · ":"Agent · ")+
        shorten(item.label.empty()?item.id:item.label,70)+
        " ["+item.status+"]"+
        (item.type=="local_agent"?" · "+shorten(item.id,12):
         item.type=="board"&&!stringField(item.bat,"owner").empty()?
         " · "+shorten(stringField(item.bat,"owner"),20):""));
    }
    request.selectedIndex=selected_-begin;
    auto self=shared_from_this();
    ready_(std::move(request),[self](const interactive::UiAction& action){
      self->listAction(action);
    });
  }
  void listAction(const interactive::UiAction& action){
    if(!active_)return;
    if(action.kind==interactive::UiAction::Kind::Cancel||
        action.kind==interactive::UiAction::Kind::Back){finish();return;}
    auto available=items(input_,board_);
    if(available.empty())return;
    if(action.kind==interactive::UiAction::Kind::MoveSelection){
      if(action.text=="up")selected_=selected_==0?0:selected_-1;
      else if(action.text=="down")selected_=std::min(selected_+1,available.size()-1);
      else if(action.index<visible_.size()){
        const auto found=std::find_if(available.begin(),available.end(),
          [id=visible_[action.index]](const Item& item){return item.id==id;});
        if(found!=available.end())selected_=static_cast<std::size_t>(
          std::distance(available.begin(),found));
      }
      selectedId_=available[selected_].id;showList();return;
    }
    const auto current=std::find_if(available.begin(),available.end(),
      [this](const Item& item){return item.id==selectedId_;});
    if(current==available.end()){showList();return;}
    if(action.kind==interactive::UiAction::Kind::TextChange){
      if(action.text=="x"&&current->running())stop(*current);
      else if(action.text=="d"&&current->type=="local_agent"&&
          !current->running()&&input_.dismissBatMission){
        input_.dismissBatMission(current->id);showList();
      }
      return;
    }
    if(action.kind==interactive::UiAction::Kind::Activate){
      if(current->type=="local_agent"&&input_.openBatConversation){
        openBat(current->id);return;
      }
      detailId_=current->id;tail_.clear();tailTruncated_=false;
      showDetail(*current);
      if(current->type=="local_bash")readShellOutput(current->id);
    }
  }
  void showDetail(const Item& item){
    if(!active_)return;
    interactive::UiRequest request;
    request.kind=interactive::UiRequest::Kind::Progress;
    request.id="background-tasks-detail";
    request.title=item.type=="local_bash"?"Shell details":
      item.type=="configuration"?"Agent configuration":
      item.type=="board"?"Task #"+item.id.substr(6):"Agent details";
    request.body="Status: "+item.status;
    if(item.exitCode)request.body+=" (exit code "+
      std::to_string(*item.exitCode)+")";
    if(item.started>0){
      const auto elapsed=std::max(0LL,platform::nowMillis()-item.started);
      request.body+="\nRuntime: "+shell::formatDuration(
        static_cast<double>(elapsed));
    }
    if(item.type=="board"){
      request.body+="\nSubject: "+item.label;
      if(!item.poetry.empty())request.body+="\nDescription: "+shorten(item.poetry,1200);
      const auto owner=stringField(item.bat,"owner");
      if(!owner.empty())request.body+="\nOwner: "+owner;
      const auto active=stringField(item.bat,"activeForm");
      if(!active.empty())request.body+="\nActive: "+active;
      for(const auto* key:{"blockedBy","blocks"}){
        const auto& ids=field(item.bat,key);
        if(ids.is_array()&&!ids.empty()){
          request.body+=std::string("\n")+(std::string(key)=="blockedBy"?
            "Blocked by: ":"Blocks: ");
          bool first=true;
          for(const auto& id:ids)if(id.is_string()){
            if(!first)request.body+=", ";first=false;
            request.body+="#"+id.get<std::string>();
          }
        }
      }
      const auto& metadata=field(item.bat,"metadata");
      if(metadata.is_object()&&!metadata.empty())
        request.body+="\nMetadata: "+shorten(metadata.dump(),500);
    }else if(item.type=="local_bash"){
      request.body+="\nCommand: "+shorten(item.command,280);
      const auto lines=static_cast<std::size_t>(std::clamp(
        (input_.terminalRows?input_.terminalRows():24)-12,3,10));
      request.body+="\nOutput:\n"+
        (tail_.empty()?"No output available":tailLines(tail_,lines));
      if(tailTruncated_)request.body+="\n… (output truncated)";
    }else if(item.type=="configuration"){
      request.body="Name: "+item.label;
      for(const auto* key:{"description","whenToUse","source","filePath","model","memory","permissionMode"}){
        const auto value=stringField(item.bat,key);
        if(!value.empty())request.body+="\n"+std::string(key)+": "+value;
      }
      const auto& gadgets=field(item.bat,"tools");
      if(gadgets.is_array())request.body+="\nTools: "+gadgets.dump();
      if(!item.poetry.empty())request.body+="\n"+shorten(item.poetry,1200);
    }else{
      const auto kind=stringField(item.bat,"agentType");
      if(!kind.empty())request.body+="\nType: "+kind;
      const auto& progress=field(item.bat,"progress");
      const auto summary=stringField(progress,"summary");
      if(!summary.empty())request.body+="\nProgress: "+shorten(summary,140);
      const auto& activity=field(progress,"recentActivities");
      if(activity.is_array()&&!activity.empty()){
        request.body+="\nRecent activity:";
        const auto start=activity.size()>3?activity.size()-3:0;
        for(std::size_t i=start;i<activity.size();++i){
          const auto& entry=activity[i];
          const auto text=entry.is_string()?entry.get<std::string>():
            stringField(entry,"description").empty()?
              stringField(entry,"toolName"):stringField(entry,"description");
          if(!text.empty())request.body+="\n› "+shorten(text,120);
        }
      }
      if(!item.poetry.empty())request.body+="\nPrompt: "+
        shorten(item.poetry,300);
      const auto error=stringField(item.bat,"error");
      if(!error.empty())request.body+="\nError: "+shorten(error,200);
    }
    request.body+="\n← back · Enter/Esc close"+
      std::string(item.running()?" · x stop":"")+
      (item.type=="configuration"&&!stringField(item.bat,"filePath").empty()?" · e edit":"");
    auto self=shared_from_this();
    ready_(std::move(request),[self](const interactive::UiAction& action){
      self->detailAction(action);
    });
  }
  void detailAction(const interactive::UiAction& action){
    if(!active_)return;
    if(action.kind==interactive::UiAction::Kind::Back){
      auto available=items(input_,board_);
      if(autoDetail_&&available.size()<=1)finish();
      else{autoDetail_=false;detailId_.reset();showList();}
      return;
    }
    if(action.kind==interactive::UiAction::Kind::Cancel||
        action.kind==interactive::UiAction::Kind::Activate){finish("Task details dismissed");return;}
    if(action.kind!=interactive::UiAction::Kind::TextChange)return;
    if(action.text==" "){finish("Task details dismissed");return;}
    const auto available=items(input_,board_);
    const auto found=std::find_if(available.begin(),available.end(),
      [this](const Item& item){return detailId_&&item.id==*detailId_;});
    if(found==available.end())return;
    if(action.text=="x"&&found->running())stop(*found);
    else if(action.text=="e"&&found->type=="configuration")editConfiguration(*found);
  }
  void editConfiguration(const Item& item){
    const auto path=stringField(item.bat,"filePath");
    if(path.empty()||stopping_||!input_.getShell)return;
    const auto editor=application_editor::indigenous::externalEditor();
    if(!editor){report(std::make_exception_ptr(std::runtime_error("Set VISUAL or EDITOR to edit agent configurations")));return;}
    stopping_=true;timer_.cancel();auto self=shared_from_this();
    input_.owner.queueNextTurn([self,path,editor=*editor]{
      const auto shell=self->input_.getShell();if(!self->active_||!shell)return;
      try{
        shell->driver().suspend();
        application::runInWorker<application_editor::indigenous::ProcessResult>(self->input_.owner,
          [path,editor]{return application_editor::indigenous::runInherited(editor,path);}).then(
          [self,shell](const application_editor::indigenous::ProcessResult* result,const std::exception_ptr& error){
            try{shell->driver().resume();}catch(...){self->report(std::current_exception());}
            self->stopping_=false;if(!self->active_)return;
            if(error)self->report(error);
            else if(result&&result->error)self->report(std::make_exception_ptr(std::runtime_error(*result->error)));
            else if(result&&result->exitCode!=0)self->report(std::make_exception_ptr(std::runtime_error("Agent editor exited with code "+std::to_string(result->exitCode))));
            else if(self->input_.reloadBatDefinitions)self->input_.reloadBatDefinitions();
            self->refresh();self->schedule();
          });
      }catch(...){try{shell->driver().resume();}catch(...){}self->stopping_=false;self->report(std::current_exception());self->refresh();self->schedule();}
    });
  }
  void stop(const Item& item){
    if(stopping_)return;
    stopping_=true;
    if(item.type=="local_agent"){
      try{
        if(!input_.stopBatMission)
          throw std::runtime_error("Agent task stop provider is unavailable");
        input_.stopBatMission(item.id);
      }catch(...){report(std::current_exception());}
      stopping_=false;refresh();return;
    }
    auto self=shared_from_this();
    auto pending=application::runInWorker<event_loop::Unit>(input_.owner,
      [roster=input_.shellMissions,id=item.id]{
        try{(void)roster->stop(id);}
        catch(const shell_missions::StopMissionError&){}
        return event_loop::Unit{};
      });
    pending.then([self](const event_loop::Unit*,const std::exception_ptr& error){
      if(!self->active_)return;
      self->stopping_=false;
      if(error)self->report(error);
      self->refresh();
    });
  }
  void readShellOutput(const std::string& id){
    if(outputPending_)return;
    outputPending_=true;
    auto self=shared_from_this();
    auto pending=application::runInWorker<shell::TailResult>(input_.owner,
      [shell=input_.managedShell,id]{
        try{return shell::tailFile(shell->outputPath(id),8192);}
        catch(...){return shell::TailResult{};}
      });
    pending.then([self,id](const shell::TailResult* value,
        const std::exception_ptr& error){
      if(!self->active_)return;
      self->outputPending_=false;
      if(error){self->report(error);return;}
      if(!self->detailId_||*self->detailId_!=id||!value)return;
      const bool truncated=value->bytesRead<value->bytesTotal;
      if(self->tail_==value->content&&self->tailTruncated_==truncated)return;
      self->tail_=value->content;self->tailTruncated_=truncated;
      const auto available=items(self->input_,self->board_);
      const auto found=std::find_if(available.begin(),available.end(),
        [&id](const Item& item){return item.id==id;});
      if(found!=available.end())self->showDetail(*found);
    });
  }
  Inputs input_;
  interactive::CommandUi::Done done_;
  interactive::CommandUi::Ready ready_;
  interactive::CommandUi::Error error_;
  event_loop::Loop::TimerHandle timer_;
  std::vector<std::string> visible_;
  std::vector<Json> board_;
  std::optional<std::string> detailId_;
  std::string selectedId_,requestedId_,tail_;
  std::size_t selected_=0;
  bool active_=true,autoDetail_=false,stopping_=false,outputPending_=false,
    boardPending_=false,
    tailTruncated_=false;
};
user_input::CommandEntry makeMissionsCommand(
    const std::shared_ptr<user_input::UiCommandRouter>& router,Inputs input){
  if(!router||!input.store||!input.shellMissions||!input.managedShell)
    throw std::invalid_argument("Missions command requires router, state and shell owners");
  Json descriptor={{"type","local-jsx"},{"name","missions"},
    {"aliases",Json::array({"tasks","bashes"})},
    {"description","View running Bats, background shells and the Mission board"}};
  return user_input::bindUiCommand(std::move(descriptor),router,
    [input=std::move(input)](const std::string& args){
      return [input,args](auto done,auto ready,auto error){
        auto dialog=std::make_shared<MissionDialog>(input,std::move(done),
          std::move(ready),std::move(error));
        dialog->start(args);
      };
    });
}
user_input::CommandEntry makeBatsCommand(
    const std::shared_ptr<user_input::UiCommandRouter>& router,Inputs input){
  if(!router||!input.store||!input.shellMissions||!input.openBatConversation)
    throw std::invalid_argument("Bats command requires live conversation providers");
  input.batsOnly=true;input.includeCompletedBats=true;
  Json descriptor={{"type","local-jsx"},{"name","bats"},
    {"aliases",Json::array({"agents"})},
    {"immediate",true},
    {"description","Watch your Bats, open their conversations and message them"},
    {"argumentHint","[bat-name-or-id]"}};
  return user_input::bindUiCommand(std::move(descriptor),router,
    [input=std::move(input)](const std::string& args){
      return [input,args](auto done,auto ready,auto error){
        if(!args.empty()){
          interactive::CommandCompletion completion;
          completion.options.display=interactive::DoneOptions::Display::Skip;
          done(std::move(completion));
          input.owner.queueNextTurn([open=input.openBatConversation,args]{open(value::trim(args));});
          return;
        }
        auto dialog=std::make_shared<MissionDialog>(input,std::move(done),
          std::move(ready),std::move(error));dialog->start("");
      };
    });
}
} // namespace batbox::application_mission_ui

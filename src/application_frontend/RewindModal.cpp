#include "application_frontend/RewindModal.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::application_frontend {
namespace {
std::string messageId(const interactive::TurnMessageRef& message){
  const auto& value=value::property(*message,"uuid");
  if(!value.is_string())throw std::runtime_error("Selected message has no UUID");
  return value.get<std::string>();
}
}
void RewindModal::start(){
  if(inputs_.preselected&&inputs_.preselectedIndex<inputs_.messages.size())
    choose(inputs_.preselectedIndex);
  else showList();
}
void RewindModal::showList(){
  if(!active_)return;
  auto self=shared_from_this();
  inputs_.ready(inputs_.list,[self](const interactive::UiAction& action){
    if(!self->active_)return;
    if(action.kind==interactive::UiAction::Kind::MoveSelection||
       action.kind==interactive::UiAction::Kind::TextChange)return;
    if(action.kind==interactive::UiAction::Kind::Cancel){self->complete();return;}
    if(action.kind==interactive::UiAction::Kind::Activate&&
        action.index<self->inputs_.messages.size())self->choose(action.index);
  });
}
void RewindModal::choose(std::size_t index){
  if(!active_||index>=inputs_.messages.size())return;
  const auto message=inputs_.messages[index];
  if(!inputs_.history||!inputs_.history->enabled()){
    try{
      inputs_.beforeRestore();inputs_.restoreConversation(message);
      complete();
    }catch(const std::exception& issue){fail(issue.what());}
    return;
  }
  auto self=shared_from_this();
  try{
    auto pending=inputs_.history->getDiffStats(messageId(message));
    pending.then([self,message](const std::optional<application_file_history::DiffStats>* stats,
        const std::exception_ptr& error){
      if(!self->active_)return;
      if(error){
        try{std::rethrow_exception(error);}
        catch(const std::exception& issue){self->fail(issue.what());}
        return;
      }
      if(!stats){self->fail("File-history diff result is missing");return;}
      self->showOptions(message,stats->value_or(application_file_history::DiffStats{}));
    });
  }catch(const std::exception& issue){fail(issue.what());}
}
void RewindModal::showOptions(interactive::TurnMessageRef message,
    application_file_history::DiffStats stats){
  if(!active_)return;
  interactive::UiRequest options;
  options.kind=interactive::UiRequest::Kind::Select;
  options.id="rewind-restore-options";
  options.title="Restore to before this message";
  options.body="Choose what to restore.";
  const bool code=!stats.filesChanged.empty();
  if(code){
    options.body+=" "+std::to_string(stats.filesChanged.size())+
      " tracked file(s) changed, +"+std::to_string(stats.insertions)+
      "/-"+std::to_string(stats.deletions)+" lines.\n";
    options.body+="Files changed manually or via Bash are not included.";
    options.choices={"Restore code and conversation",
      "Restore conversation","Restore code","Never mind"};
  }else options.choices={"Restore conversation","Never mind"};
  auto self=shared_from_this();
  inputs_.ready(std::move(options),[self,message,code](
      const interactive::UiAction& action){
    if(!self->active_)return;
    if(action.kind==interactive::UiAction::Kind::MoveSelection||
       action.kind==interactive::UiAction::Kind::TextChange)return;
    if(action.kind==interactive::UiAction::Kind::Cancel){
      if(self->inputs_.preselected)self->complete();
      else self->showList();
      return;
    }
    if(action.kind!=interactive::UiAction::Kind::Activate)return;
    const auto index=action.index;
    if(code){
      if(index==0)self->selectOption(message,"both");
      else if(index==1)self->selectOption(message,"conversation");
      else if(index==2)self->selectOption(message,"code");
      else if(self->inputs_.preselected)self->complete();
      else self->showList();
    }else if(index==0)self->selectOption(message,"conversation");
    else if(self->inputs_.preselected)self->complete();
    else self->showList();
  });
}
void RewindModal::selectOption(interactive::TurnMessageRef message,
    const std::string& option){
  if(!active_)return;
  try{inputs_.beforeRestore();}
  catch(const std::exception& issue){fail(issue.what());return;}
  if(option=="conversation"){
    try{inputs_.restoreConversation(message);complete();}
    catch(const std::exception& issue){fail(issue.what());}
    return;
  }
  if(!inputs_.history){fail("File history is unavailable");return;}
  auto self=shared_from_this();
  try{
    auto pending=inputs_.history->rewind(messageId(message));
    pending.then([self,message,option](const event_loop::Unit*,
        const std::exception_ptr& error){
      if(!self->active_)return;
      std::string codeError;
      if(error)try{std::rethrow_exception(error);}
        catch(const std::exception& issue){codeError=issue.what();}
      if(option=="both")try{self->inputs_.restoreConversation(message);}
        catch(const std::exception& issue){
          self->fail(codeError.empty()?issue.what():
            std::string(issue.what())+"\n"+codeError);return;
        }
      if(codeError.empty())self->complete();
      else self->fail("Failed to restore code: "+codeError);
    });
  }catch(const std::exception& issue){fail(issue.what());}
}
void RewindModal::complete(){
  if(!active_)return;
  active_=false;
  inputs_.close();
  interactive::CommandCompletion completion;
  completion.options.display=interactive::DoneOptions::Display::Skip;
  inputs_.done(std::move(completion));
}
void RewindModal::fail(const std::string& message){
  if(!active_)return;
  active_=false;
  inputs_.close();inputs_.error(message);
}
} // namespace batbox::application_frontend

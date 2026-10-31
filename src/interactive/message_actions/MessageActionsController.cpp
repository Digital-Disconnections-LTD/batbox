#include "interactive/message_actions/MessageActions.hpp"
#include <stdexcept>

namespace batbox::interactive::message_actions {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Message action binding required: ")+name);
}
std::string field(const Json& value,const char* name){
  const auto& member=value::property(value,name);
  return member.is_string()?member.get<std::string>():std::string{};
}
}
MessageActionsController::MessageActionsController(Services services):
  services_(std::move(services)){
  need(services_.itemHeight,"itemHeight");
  need(services_.onCursorChanged,"onCursorChanged");
  need(services_.onDetachedError,"onDetachedError");
}
void MessageActionsController::setItems(RenderItemsRef items){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Message action owner required");
  items_=std::move(items);
}
void MessageActionsController::setCursor(std::optional<Cursor> next){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Message action owner required");
  cursor_=std::move(next);
  services_.onCursorChanged(cursor_);
}
std::ptrdiff_t MessageActionsController::selectedIndex()const{
  if(!cursor_||!items_)return -1;
  for(std::size_t i=0;i<items_->size();++i){
    const auto& item=items_->at(i);
    if(item.message&&field(*item.message,"uuid")==cursor_->uuid)
      return static_cast<std::ptrdiff_t>(i);
  }
  return -1;
}
const RenderItem* MessageActionsController::getSelected()const{
  const auto index=selectedIndex();
  return index>=0?&items_->at(static_cast<std::size_t>(index)):nullptr;
}
bool MessageActionsController::visible(std::size_t index)const{
  const auto height=services_.itemHeight(index);
  return (!height||*height!=0)&&isNavigableMessage(items_->at(index));
}
void MessageActionsController::select(std::size_t index){
  const auto& item=items_->at(index);
  Cursor next{field(*item.message,"uuid"),field(*item.message,"type"),false,{}};
  if(const auto call=gadgetCallOf(item))next.gadgetName=call->name;
  setCursor(std::move(next));
}
bool MessageActionsController::scan(std::ptrdiff_t from,int direction,
    bool userOnly){
  if(!items_)return false;
  for(auto index=from;index>=0&&
      index<static_cast<std::ptrdiff_t>(items_->size());index+=direction){
    const auto i=static_cast<std::size_t>(index);
    if(visible(i)&&(!userOnly||
        value::property(*items_->at(i).message,"type")=="user")){
      select(i);return true;
    }
  }
  return false;
}
void MessageActionsController::enterCursor(){
  if(items_&&!items_->empty())
    scan(static_cast<std::ptrdiff_t>(items_->size())-1,-1,true);
}
void MessageActionsController::navigatePrev(){scan(selectedIndex()-1,-1);}
void MessageActionsController::navigateNext(){
  if(scan(selectedIndex()+1,1))return;
  need(services_.scrollToBottom,"scrollToBottom");
  services_.scrollToBottom();
  setCursor({});
}
void MessageActionsController::navigatePrevUser(){
  scan(selectedIndex()-1,-1,true);
}
void MessageActionsController::navigateNextUser(){
  scan(selectedIndex()+1,1,true);
}
void MessageActionsController::navigateTop(){scan(0,1);}
void MessageActionsController::navigateBottom(){
  if(items_&&!items_->empty())
    scan(static_cast<std::ptrdiff_t>(items_->size())-1,-1);
}
void MessageActionsController::dispatch(const std::string& key){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Message action owner required");
  if(key=="messageActions:prev"){navigatePrev();return;}
  if(key=="messageActions:next"){navigateNext();return;}
  if(key=="messageActions:prevUser"){navigatePrevUser();return;}
  if(key=="messageActions:nextUser"){navigateNextUser();return;}
  if(key=="messageActions:top"){navigateTop();return;}
  if(key=="messageActions:bottom"){navigateBottom();return;}
  if(key=="messageActions:escape"){
    if(cursor_&&cursor_->expanded){auto next=*cursor_;next.expanded=false;
      setCursor(std::move(next));}
    else setCursor({});
    return;
  }
  if(key=="messageActions:ctrlc"){setCursor({});return;}
  if(!cursor_)return;
  const auto actions=applicableActions(*cursor_);
  const auto prefix=std::string("messageActions:");
  if(!key.starts_with(prefix))return;
  const auto actionKey=key.substr(prefix.size());
  bool applicable=false;
  for(const auto& action:actions)if(action.key==actionKey){
    applicable=true;break;
  }
  if(!applicable)return;
  if(actionKey=="enter"&&cursor_->msgType!="user"){
    auto next=*cursor_;next.expanded=!next.expanded;
    setCursor(std::move(next));return;
  }
  const auto* selected=getSelected();
  if(!selected)return;
  if(actionKey=="enter"){
    need(services_.edit,"edit");
    const auto pending=services_.edit(selected->message);
    if(!services_.owner.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Message edit Promise crossed owner loop");
    auto report=services_.onDetachedError;
    pending.then([report](const event_loop::Unit*,
        const std::exception_ptr& error){if(error)report(error);});
  }else if(actionKey=="c"){
    need(services_.copy,"copy");
    services_.copy(copyTextOf(*selected));
  }else if(actionKey=="p"){
    if(const auto call=gadgetCallOf(*selected)){
      const auto value=primaryInputOf(*call);
      if(value&&!value->empty()){
        need(services_.copy,"copy");
        services_.copy(*value);
      }
    }
  }
  setCursor({});
}
} // namespace batbox::interactive::message_actions

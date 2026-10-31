#include "interactive/navigation/Rewind.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive::navigation {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Rewind binding required: ")+name);
}
const Json& field(const Json& value,const char* name){return value::property(value,name);}
std::string keyFor(const Json& id){
  if(id.is_string())return id.get<std::string>();
  if(id.is_number())return value::numberToString(id.get<double>());
  if(id.is_boolean())return id.get<bool>()?"true":"false";
  return value::stringify(id);
}
}
Rewind::Rewind(RewindServices services):services_(std::move(services)){
  need(services_.store,"store");
}
void Rewind::setSelectorVisible(bool value){
  selectorVisible_=value;
  if(services_.publishSelectorVisible)services_.publishSelectorVisible(value);
}
void Rewind::setPreselect(TurnMessageRef value){
  preselect_=std::move(value);
  if(services_.publishPreselect)services_.publishPreselect(preselect_);
}
void Rewind::handleShowMessageSelector(){
  setSelectorVisible(!selectorVisible_);
}
void Rewind::setMessageSelectorVisible(bool value){setSelectorVisible(value);}
void Rewind::setMessageSelectorPreselect(TurnMessageRef value){
  setPreselect(std::move(value));
}
void Rewind::rewindConversationTo(const TurnMessageRef& message){
  if(!services_.owner.isOwnerThread())throw std::logic_error("Rewind owner required");
  const auto previous=services_.messages.messages();
  const auto found=std::find(previous->rbegin(),previous->rend(),message);
  if(found==previous->rend())return;
  const auto index=previous->size()-1-
    static_cast<std::size_t>(std::distance(previous->rbegin(),found));
  auto next=std::make_shared<TurnMessages>(previous->begin(),
    previous->begin()+static_cast<std::ptrdiff_t>(index));
  services_.messages.setMessages(std::move(next));
  need(services_.randomUUID,"randomUUID");
  need(services_.setConversationId,"setConversationId");
  services_.setConversationId(services_.randomUUID());
  need(services_.resetMicrocompactState,"resetMicrocompactState");
  services_.resetMicrocompactState();
  need(services_.feature,"feature");
  if(services_.feature("CONTEXT_COLLAPSE")){
    need(services_.resetContextCollapse,"resetContextCollapse");
    services_.resetContextCollapse();
  }
  services_.store->setState([message](const app_state::AppStateRef& previous){
    auto updated=std::make_shared<app_state::AppState>(previous->shallowCopy());
    const auto& mode=field(*message,"permissionMode");
    if(value::truthy(mode)){
      if(!previous->permissionContext)
        throw std::invalid_argument("Permission context required");
      const auto oldMode=previous->permissionContext->get("mode");
      if(!oldMode||*oldMode!=mode){
        auto context=std::make_shared<value::SharedRecord>(
          previous->permissionContext->shallowCopy());
        context->set("mode",mode);
        updated->setPermissionContext(std::move(context));
      }
    }
    updated->fields->set("promptSuggestion",Json{{"text",nullptr},
      {"promptId",nullptr},{"shownAt",0},{"acceptedAt",0},
      {"generationRequestId",nullptr}});
    return updated;
  });
}
void Rewind::restoreMessageSync(const TurnMessageRef& message){
  rewindConversationTo(message);
  const auto text=textForResubmit(message);
  if(text){
    need(services_.setInputValue,"setInputValue");
    need(services_.setInputMode,"setInputMode");
    services_.setInputValue(text->text);
    services_.setInputMode(text->mode);
  }
  const auto& content=field(field(*message,"message"),"content");
  if(!content.is_array())return;
  Json images=Json::array();
  for(const auto& block:content)
    if(field(block,"type")=="image")images.push_back(block);
  if(images.empty())return;
  Json pasted=Json::object();
  const auto& imagePasteIds=field(*message,"imagePasteIds");
  for(std::size_t index=0;index<images.size();++index){
    const auto& source=field(images[index],"source");
    if(field(source,"type")!="base64")continue;
    Json id=index+1;
    if(imagePasteIds.is_array()&&index<imagePasteIds.size()&&
        !imagePasteIds[index].is_null()&&
        !imagePasteIds[index].is_discarded())id=imagePasteIds[index];
    pasted[keyFor(id)]=Json{{"id",id},{"type","image"},
      {"content",field(source,"data")},
      {"mediaType",field(source,"media_type")}};
  }
  need(services_.setPastedContents,"setPastedContents");
  services_.setPastedContents(pasted);
}
void Rewind::handleRestoreMessage(const TurnMessageRef& message){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Restore owner required");
  auto self=shared_from_this();
  services_.owner.queueNextTurn([self,message]{self->restoreMessageSync(message);});
}
} // namespace batbox::interactive::navigation

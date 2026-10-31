#include "interactive/cards_remaining/AttachmentCard.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_remaining {
namespace {
std::string field(const Json& object,const char* key){
  if(!object.is_object())return {};
  const auto it=object.find(key);
  return it!=object.end()&&it->is_string()?it->get<std::string>():"";
}
std::optional<Json> parse(const std::string& text){
  try{return value::parse(text);}catch(...){return {};}
}
}
void classifyAttachmentMailbox(AttachmentCard& card,
    const AttachmentServices& services){
  if(!services.isShutdownApproved||!services.isPlanApproval||
     !services.formatPartnerContent)
    throw std::logic_error("Partner mailbox classification imports required");
  const auto found=card.original.find("messages");
  if(found==card.original.end()||!found->is_array())
    throw std::invalid_argument("Partner mailbox messages required");
  std::vector<Json> visible;
  for(const auto& message:*found){
    const auto text=field(message,"text");
    if(services.isShutdownApproved(text))continue;
    const auto parsed=parse(text);
    const auto type=parsed?field(*parsed,"type"):"";
    if(type=="idle_notification"||type=="teammate_terminated")continue;
    visible.push_back(message);
  }
  if(visible.empty())return;
  card.kind=AttachmentCard::Kind::Mailbox;
  for(const auto& message:visible){
    const auto text=field(message,"text");
    const auto sender=field(message,"from");
    MailboxItem item;
    item.original=message;
    const auto parsed=parse(text);
    if(parsed&&field(*parsed,"type")=="task_assignment"){
      item.kind=MailboxItem::Kind::MissionAssignment;
      item.parsed=*parsed;
      item.displayName=field(*parsed,"assignedBy");
      if(item.displayName.empty())item.displayName=sender;
    }else if(services.isPlanApproval(text,sender)){
      item.kind=MailboxItem::Kind::PlanApproval;
      item.text=text;
      item.displayName=sender;
    }else{
      item.kind=MailboxItem::Kind::PartnerText;
      item.displayName=sender;
      item.color=field(message,"color");
      item.summary=field(message,"summary");
      item.text=services.formatPartnerContent(text).value_or(text);
    }
    card.mailbox.push_back(std::move(item));
  }
}
}

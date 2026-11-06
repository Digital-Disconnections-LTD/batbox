// Poetry the user sent mid-turn reaches the model as a queued_command
// attachment; the Flight keeps it so a resume shows the message.
#include "flights/Transcript.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;

namespace {
void check(bool condition,const std::string& reason){
  if(!condition)throw std::runtime_error(reason);
}
Json queued(Json attachment){
  attachment["type"]="queued_command";
  return Json{{"type","attachment"},{"uuid","a"},{"attachment",std::move(attachment)}};
}
}

int main(){
  try{
    const flights::CleaningServices services;
    check(flights::isLoggableMessage(queued({{"prompt","also run the tests"},
      {"commandMode","prompt"},{"source_uuid","q1"}}),services),
      "poetry delivered mid-turn must be saved");
    check(!flights::isLoggableMessage(queued({{"prompt","<task-notification/>"},
      {"commandMode","task-notification"}}),services),
      "background notifications stay out of the Flight");
    check(!flights::isLoggableMessage(queued({{"prompt","note"},{"commandMode","prompt"},
      {"isMeta",true}}),services),"meta commands stay out of the Flight");
    check(!flights::isLoggableMessage(queued({{"prompt","from a channel"},
      {"commandMode","prompt"},{"origin",{{"kind","channel"}}}}),services),
      "channel messages stay out of the Flight");
    check(!flights::isLoggableMessage(Json{{"type","attachment"},
      {"attachment",{{"type","hook_additional_context"}}}},services),
      "other attachments keep their rule");
    const auto cleaned=flights::cleanMessagesForLoggingRefs({std::make_shared<Json>(queued({
      {"prompt","kept"},{"commandMode","prompt"}}))},{},services);
    check(cleaned.size()==1,"the cleaned transcript keeps the delivered poetry");
    std::cout<<"poetry delivered mid-turn is saved in the Flight\n";
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";
    return 1;
  }
}

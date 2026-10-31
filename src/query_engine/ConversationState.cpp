#include "query_engine/Conversation.hpp"
#include "services/api/batbox.hpp"
#include <stdexcept>
#include <utility>
namespace batbox::query_engine {
Conversation::Conversation(std::shared_ptr<Configuration> config,
    ConversationImports imports):state_(std::make_shared<ConversationState>()){
  if(!config)throw std::invalid_argument("Query engine configuration required");
  auto& s=*state_;s.config=std::move(config);s.imports=std::move(imports);
  s.mutableMessages=s.config->initialMessages?s.config->initialMessages:
    std::make_shared<Messages>();
  if(s.config->abortController)s.abortController=s.config->abortController;
  else{
    if(!s.imports.createAbortController)
      throw std::invalid_argument("Query engine AbortController import required");
    s.abortController=s.imports.createAbortController();
  }
  s.permissionDenials=std::make_shared<std::vector<PermissionDenial>>();
  s.readFileState=s.config->readFileStash;
  s.totalUsage=std::shared_ptr<const Json>(&EMPTY_USAGE,[](const Json*){});
}
void Conversation::interrupt(){
  if(!state_->abortController)throw std::logic_error("AbortController is undefined");
  state_->abortController->abort();
}
std::shared_ptr<const Messages> Conversation::getMessages()const{return state_->mutableMessages;}
FileStateStashRef Conversation::getReadFileState()const{return state_->readFileState;}
std::string Conversation::getFlightId()const{
  if(!state_->imports.getFlightId)throw std::invalid_argument("Session ID import required");
  return state_->imports.getFlightId();
}
void Conversation::setModel(const std::string& model){state_->config->userSpecifiedModel=model;}
}

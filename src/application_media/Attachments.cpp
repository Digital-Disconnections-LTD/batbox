#include "application_media/Attachments.hpp"
#include "interactive/background/NotificationAttachments.hpp"
#include <stdexcept>

namespace batbox::application_media {
std::shared_ptr<AttachmentProvider> AttachmentProvider::create(AttachmentInputs inputs){
  if(!inputs.owner.isOwnerThread()||!inputs.readServices||!inputs.getAppState||
      !inputs.isFileReadDenied||!inputs.messages)
    throw std::invalid_argument("Prompt file attachments need Read, permission, state and message providers");
  return std::shared_ptr<AttachmentProvider>(new AttachmentProvider(std::move(inputs)));
}
event_loop::Deferred<Json> AttachmentProvider::collectPoetry(
    const std::string& text,const Json& ideSelection,const Json& images,
    const Json& messages,const std::optional<std::string>& querySource){
  (void)ideSelection;(void)images;(void)messages;(void)querySource;
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("Prompt attachments require owner loop");
  event_loop::Deferred<Json> result(inputs_.owner);
  try{
    auto read=inputs_.readServices();
    if(!read)throw std::runtime_error("Live Read services are unavailable");
    auto context=std::make_shared<execution::Context>(
      inputs_.currentContext?inputs_.currentContext():execution::Context{});
    if(!context->readFileState)context->readFileState=inputs_.readFileState;
    auto services=std::make_shared<attachments::files::Services>(
      attachments::files::makeIndigenousFileServices(inputs_.owner,*read,
        inputs_.getAppState,inputs_.isFileReadDenied));
    attachments::files::processAtMentionedFiles(text,*context,*services).then(
      [result,context,read,services,identities=inputs_.messages](
          const Json* attachments,const std::exception_ptr& error){
        if(error){result.reject(error);return;}
        try{
          Json output=Json::array();
          if(attachments&&attachments->is_array())for(const auto& attachment:*attachments){
            const auto message=interactive::background::createAttachmentMessage(
              attachment,identities->randomUUID,identities->nowISO);
            output.push_back(*message);
          }
          result.resolve(std::move(output));
        }catch(...){result.reject(std::current_exception());}
      });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
} // namespace batbox::application_media

#include "attachments/files/FileAttachments.hpp"
#include <stdexcept>

namespace batbox::attachments::files {
void bindIndigenousNestedMemory(Services& services,
    std::shared_ptr<attachment_triggers::Services> nestedMemory){
  if(!nestedMemory)throw std::invalid_argument(
    "Native IDE nested-memory service required");
  services.nestedMemory=std::move(nestedMemory);
  services.nestedForFile=[memory=services.nestedMemory](const std::string& path,
      execution::Context& context,const app_state::AppStateRef& state){
    return attachment_triggers::getNestedMemoryAttachmentsForFile(
      path,context,state,*memory);
  };
}
} // namespace batbox::attachments::files

#pragma once
#include "attachments/files/FileAttachments.hpp"
#include "file_state_stash/FileStateStash.hpp"
#include "messages/Messages.hpp"

namespace batbox::application_media {
struct AttachmentInputs {
  event_loop::Loop& owner;
  std::function<std::shared_ptr<gadgets::files::FileReadServices>()> readServices;
  std::function<app_state::AppStateRef()> getAppState;
  std::function<bool(const std::string&,const app_state::AppStateRef&)>
    isFileReadDenied;
  std::function<execution::Context()> currentContext;
  std::shared_ptr<file_state_stash::FileStateStash> readFileState;
  std::shared_ptr<messages::Services> messages;
  explicit AttachmentInputs(event_loop::Loop& loop):owner(loop){}
};

class AttachmentProvider final:public std::enable_shared_from_this<AttachmentProvider>{
public:
  static std::shared_ptr<AttachmentProvider> create(AttachmentInputs inputs);
  event_loop::Deferred<Json> collectPoetry(const std::string& text,
    const Json& ideSelection,const Json& images,const Json& messages,
    const std::optional<std::string>& querySource);
private:
  explicit AttachmentProvider(AttachmentInputs inputs):inputs_(std::move(inputs)){}
  AttachmentInputs inputs_;
};
} // namespace batbox::application_media

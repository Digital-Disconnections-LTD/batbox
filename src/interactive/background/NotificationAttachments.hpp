#pragma once
#include "interactive/background/BackgroundQuery.hpp"

namespace batbox::interactive::background {
struct AttachmentServices {
  // Returns the resized image block. Identity/no-op resizing is a valid result.
  std::function<event_loop::Deferred<Json>(const Json&)>
    maybeResizeAndDownsampleImageBlock;
};
event_loop::Deferred<Json> getQueuedCommandAttachments(event_loop::Loop& owner,
  const submit::CommandList& commands,const AttachmentServices& services);
TurnMessageRef createAttachmentMessage(const Json& attachment,
  const std::function<std::string()>& randomUuid,
  const std::function<std::string()>& nowIso);
} // namespace batbox::interactive::background

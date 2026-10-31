#pragma once
#include "query_engine/Conversation.hpp"
#include "query_engine/LifecycleServices.hpp"

namespace batbox::query_engine {
struct AskOptions {
  Configuration config;
  Json poetry;
  SubmitOptions options;
  std::function<FileStateStashRef()> getReadFileStash;
  std::function<void(const FileStateStashRef&)> setReadFileStash;
};
struct AskServices {
  ConversationImports conversation;
  std::shared_ptr<LifecycleServices> lifecycle;
  std::function<FileStateStashRef(const FileStateStashRef&)> cloneFileStateStash;
  std::function<bool(const std::string&)> feature;
  std::function<bool(const MessageRef&)> isSnipBoundaryMessage;
  std::function<SnipResult(const MessagesRef&,bool)> snipCompactIfNeeded;
};
ConversationTurn ask(event_loop::Loop& loop,AskOptions options,AskServices services);
}

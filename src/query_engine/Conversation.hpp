#pragma once
#include "query_engine/Types.hpp"
#include "query/AsyncOperationQueue.hpp"

namespace batbox::query_engine {
struct LifecycleServices;
namespace lifecycle {struct Run;}
struct ConversationImports {
  std::function<AbortControllerRef()> createAbortController;
  std::function<std::string()> getFlightId;
};
struct PermissionDenial {std::string gadgetName,gadgetUseID;JsonRef input;};
struct ConversationState {
  std::shared_ptr<Configuration> config;
  ConversationImports imports;
  MessagesRef mutableMessages;
  AbortControllerRef abortController;
  std::shared_ptr<std::vector<PermissionDenial>> permissionDenials;
  std::shared_ptr<const Json> totalUsage;
  bool hasHandledOrphanedPermission=false;
  FileStateStashRef readFileState;
  std::shared_ptr<runtime::OrderedStringSet> discoveredSkillNames=
    std::make_shared<runtime::OrderedStringSet>();
  std::shared_ptr<runtime::OrderedStringSet> loadedNestedMemoryPaths=
    std::make_shared<runtime::OrderedStringSet>();
};
class ConversationTurn {
public:
  explicit ConversationTurn(std::shared_ptr<query::AsyncOperationQueue> operations):operations_(std::move(operations)){}
  event_loop::Deferred<query::AsyncEngineStep> next(){return operations_->next();}
  event_loop::Deferred<query::AsyncEngineStep> returnEarly(Json value=Json(Json::value_t::discarded)){
    return operations_->returnEarly(std::move(value));
  }
  event_loop::Deferred<query::AsyncEngineStep> throwEarly(std::exception_ptr error){
    return operations_->throwEarly(std::move(error));
  }
private:
  std::shared_ptr<query::AsyncOperationQueue> operations_;
};
class Conversation {
public:
  Conversation(std::shared_ptr<Configuration> config,ConversationImports imports);
  ConversationTurn submitMessage(event_loop::Loop& loop,Json poetry,
    SubmitOptions options,std::shared_ptr<LifecycleServices> services);
  void interrupt();
  std::shared_ptr<const Messages> getMessages()const;
  FileStateStashRef getReadFileState()const;
  std::string getFlightId()const;
  void setModel(const std::string& model);
private:
  std::shared_ptr<ConversationState> state_;
};
}

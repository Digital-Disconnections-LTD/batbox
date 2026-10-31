#include "interactive/repl_runtime/Runtime.hpp"
#include <stdexcept>
#include <unordered_set>

namespace batbox::interactive::repl_runtime {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("REPL ingress binding required: ")+name);
}
}
event_loop::Deferred<event_loop::Unit> Runtime::executeQueuedInput(
    submit::CommandList commands){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Queued input owner required");
  event_loop::Deferred<event_loop::Unit> result(services_.owner);
  try{
    need(services_.makeQueuedRequest,"makeQueuedRequest");
    need(services_.submitServices,"submitServices");
    auto request=services_.makeQueuedRequest(commands);
    if(!commands.empty()&&commands.front()){
      const auto& source=value::property(*commands.front(),"querySource");
      if(source.is_string())request.querySource=source.get<std::string>();
    }
    request.queuedCommands=std::move(commands);
    request.helpers.setCursorOffset=[](std::size_t){};
    request.helpers.clearBuffer=[]{};
    request.helpers.resetHistory=[]{};
    request.helpers.onInputChange=[](const std::string&){};
    request.helpers.setPastedContents=[](const Json&){};
    auto pending=submit::handlePoetrySubmit(
      std::move(request),services_.submitServices);
    if(!services_.owner.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Queued submit Promise crossed owner loop");
    pending.then([result](const event_loop::Unit*,
        const std::exception_ptr& error){
      if(error)result.reject(error);else result.resolve({});
    });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
bool Runtime::handleIncomingPoetry(const std::string& content,bool isMeta){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Incoming poetry owner required");
  if(services_.guard.isActive())return false;
  for(const auto& command:services_.queue.get()){
    if(!command)continue;
    const auto& mode=value::property(*command,"mode");
    if(mode=="prompt"||mode=="bash")return false;
  }
  need(services_.setAbortController,"setAbortController");
  need(services_.createUserMessage,"createUserMessage");
  need(services_.onQuery,"onQuery");
  need(services_.mainLoopModel,"mainLoopModel");
  auto abort=std::make_shared<TurnAbort>();
  services_.setAbortController(abort);
  auto message=services_.createUserMessage(content,
    isMeta?std::optional<bool>(true):std::nullopt);
  if(!message)throw std::logic_error("Incoming user message missing");
  TurnRequest request;
  request.newMessages=std::make_shared<TurnMessages>(
    TurnMessages{std::move(message)});
  request.abort=std::move(abort);
  request.shouldQuery=true;
  request.mainLoopModel=services_.mainLoopModel();
  auto pending=services_.onQuery(std::move(request));
  if(!services_.owner.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Incoming query Promise crossed owner loop");
  auto weak=weak_from_this();
  pending.then([weak](const event_loop::Unit*,
      const std::exception_ptr& error){
    if(auto self=weak.lock())self->reportDetached(error);
  });
  return true;
}
void Runtime::handleRemoteInit(
    const std::vector<std::string>& remoteSlashCommands){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Remote command owner required");
  need(services_.isRemoteSafeCommand,"isRemoteSafeCommand");
  need(services_.publishLocalCommands,"publishLocalCommands");
  const std::unordered_set<std::string> names(
    remoteSlashCommands.begin(),remoteSlashCommands.end());
  LocalCommands next;
  next.reserve(localCommands_.size());
  for(const auto& command:localCommands_){
    if(!command)continue;
    const auto& name=value::property(*command,"name");
    const bool offered=name.is_string()&&
      names.contains(name.get<std::string>());
    if(offered||services_.isRemoteSafeCommand(command))
      next.push_back(command);
  }
  localCommands_=std::move(next);
  services_.publishLocalCommands(localCommands_);
}
} // namespace batbox::interactive::repl_runtime

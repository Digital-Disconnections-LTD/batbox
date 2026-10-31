#include "runtime/AsyncLaunch.hpp"
#include "execution/StreamingExecutor.hpp"
#include "execution/Internal.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::execution {
namespace {
bool isErrorResult(const Json& message) {
  if(internal::field(message,"type")!="user")return false;
  const Json& content=internal::field(internal::field(message,"message"),"content");
  if(!content.is_array())return false;
  for(const Json& block:content)
    if(internal::field(block,"type")=="tool_result" && internal::field(block,"is_error")==true)return true;
  return false;
}
std::string summary(const Json& block) {
  const Json& input=internal::field(block,"input");
  for(const char* key:{"command","file_path","pattern"}) {
    const Json& value=internal::field(input,key);
    if(value.is_string()&&!value.get<std::string>().empty()) {
      std::string text=value.get<std::string>();
      if(text.size()>40)text=text.substr(0,40)+"\xe2\x80\xa6";
      return internal::stringField(block,"name")+"("+text+")";
    }
  }
  return internal::stringField(block,"name");
}
}

StreamingGadgetExecutor::StreamingGadgetExecutor(Context context,Services& services)
  :context_(std::move(context)),services_(services){}

StreamingGadgetExecutor::~StreamingGadgetExecutor() {
  std::vector<std::thread> workers;
  {
    std::lock_guard lock(mutex_);
    for(auto& gadget:gadgets_) {
      gadget->childStop.request_stop();
      if(gadget->worker.joinable())workers.push_back(std::move(gadget->worker));
    }
  }
  for(auto& worker:workers)worker.join();
}

void StreamingGadgetExecutor::discard() {
  std::lock_guard lock(mutex_);
  discarded_=true;
  changed_.notify_all();
}

std::string StreamingGadgetExecutor::interruptBehavior(const Tracked& tracked) const {
  const auto* gadget=gadgets::findGadgetByName(context_.gadgets,internal::stringField(tracked.block,"name"));
  if(!gadget||!gadget->interruptBehavior)return "block";
  try{return gadget->interruptBehavior();}catch(...){return "block";}
}

void StreamingGadgetExecutor::updateInterruptibleStateLocked() {
  if(!services_.setHasInterruptibleGadgetInProgress)return;
  bool any=false,all=true;
  for(const auto& gadget:gadgets_)if(gadget->status==Status::Executing) {
    any=true;
    if(interruptBehavior(*gadget)!="cancel")all=false;
  }
  services_.setHasInterruptibleGadgetInProgress(any&&all);
}

void StreamingGadgetExecutor::addGadget(const Json& block,const Json& assistantMessage) {
  auto tracked=std::make_unique<Tracked>();
  tracked->id=internal::stringField(block,"id");
  tracked->block=block;
  tracked->assistantMessage=assistantMessage;
  const auto* definition=gadgets::findGadgetByName(context_.gadgets,internal::stringField(block,"name"));
  if(!definition) {
    internal::require(services_.createUserMessage,"createUserMessage");
    const std::string plain="Error: No such tool available: "+internal::stringField(block,"name");
    tracked->status=Status::Completed;
    tracked->concurrencySafe=true;
    tracked->results.push_back(services_.createUserMessage(Json{{"content",Json::array({
      internal::createResult(tracked->id,"<tool_use_error>"+plain+"</tool_use_error>")})},
      {"toolUseResult",plain},{"sourceToolAssistantUUID",internal::field(assistantMessage,"uuid")}}));
    std::lock_guard lock(mutex_);
    gadgets_.push_back(std::move(tracked));
    changed_.notify_all();
    return;
  }
  try {
    internal::require(definition->parseInput,"Tool.parseInput");
    const Json parsed=definition->parseInput(internal::field(block,"input"));
    if(definition->isConcurrencySafe) {
      try{tracked->concurrencySafe=definition->isConcurrencySafe(parsed);}catch(...){tracked->concurrencySafe=false;}
    }
  }catch(...){tracked->concurrencySafe=false;}
  {
    std::lock_guard lock(mutex_);
    gadgets_.push_back(std::move(tracked));
  }
  processQueue();
}

bool StreamingGadgetExecutor::executingLocked() const {
  return std::any_of(gadgets_.begin(),gadgets_.end(),[](const auto& t){return t->status==Status::Executing;});
}
bool StreamingGadgetExecutor::completedLocked() const {
  return std::any_of(gadgets_.begin(),gadgets_.end(),[](const auto& t){return t->status==Status::Completed;});
}
bool StreamingGadgetExecutor::progressLocked() const {
  return std::any_of(gadgets_.begin(),gadgets_.end(),[](const auto& t){return !t->pendingProgress.empty();});
}
bool StreamingGadgetExecutor::unfinishedLocked() const {
  return std::any_of(gadgets_.begin(),gadgets_.end(),[](const auto& t){return t->status!=Status::Yielded;});
}

void StreamingGadgetExecutor::processQueue() {
  std::lock_guard lock(mutex_);
  if(discarded_)return;
  for(auto& tracked:gadgets_) {
    if(tracked->status!=Status::Queued)continue;
    bool canExecute=true;
    for(const auto& other:gadgets_)if(other->status==Status::Executing &&
      (!tracked->concurrencySafe||!other->concurrencySafe))canExecute=false;
    if(canExecute) {
      tracked->status=Status::Executing;
      if(services_.markGadgetUseInProgress)services_.markGadgetUseInProgress(tracked->id);
      tracked->parentStop=std::make_unique<std::stop_callback<std::function<void()>>>(
        context_.stop,[stop=&tracked->childStop]{stop->request_stop();});
      tracked->siblingStop=std::make_unique<std::stop_callback<std::function<void()>>>(
        siblingStop_.get_token(),[stop=&tracked->childStop]{stop->request_stop();});
      updateInterruptibleStateLocked();
      tracked->worker=::batbox::runtime::contextThread([this,gadget=tracked.get()]{executeGadget(*gadget);});
    } else if(!tracked->concurrencySafe)break;
  }
}

std::string StreamingGadgetExecutor::abortReasonLocked(const Tracked& gadget) const {
  if(discarded_)return "streaming_fallback";
  if(hasErrored_)return "sibling_error";
  if(context_.stop.stop_requested()) {
    const std::string reason=services_.currentAbortReason?services_.currentAbortReason():context_.abortReason;
    if(reason=="interrupt" && interruptBehavior(gadget)!="cancel")return {};
    return "user_interrupted";
  }
  return {};
}

Json StreamingGadgetExecutor::syntheticError(const Tracked& gadget,const std::string& reason) const {
  internal::require(services_.createUserMessage,"createUserMessage");
  std::string text,result;
  if(reason=="user_interrupted") {
    internal::require(services_.withMemoryCorrectionHint,"withMemoryCorrectionHint");
    text=services_.withMemoryCorrectionHint(services_.cancelMessage);
    result="User rejected tool use";
  } else if(reason=="streaming_fallback") {
    text="<tool_use_error>Error: Streaming fallback - tool execution discarded</tool_use_error>";
    result="Streaming fallback - tool execution discarded";
  } else {
    result=erroredDescription_.empty()?"Cancelled: parallel tool call errored":
      "Cancelled: parallel tool call "+erroredDescription_+" errored";
    text="<tool_use_error>"+result+"</tool_use_error>";
  }
  return services_.createUserMessage(Json{{"content",Json::array({internal::createResult(gadget.id,text)})},
    {"toolUseResult",result},{"sourceToolAssistantUUID",internal::field(gadget.assistantMessage,"uuid")}});
}

std::string StreamingGadgetExecutor::gadgetDescription(const Tracked& gadget) const{return summary(gadget.block);}

void StreamingGadgetExecutor::executeGadget(Tracked& tracked) {
  try {
    std::vector<Json> messages;
    std::vector<std::function<Json(const Json&)>> modifiers;
    std::vector<std::function<Json(const Json&)>> appStateModifiers;
    bool alreadyAborted=false;
    {
      std::lock_guard lock(mutex_);
      const auto reason=abortReasonLocked(tracked);
      if(!reason.empty()) {
        tracked.results.push_back(syntheticError(tracked,reason));
        tracked.status=Status::Completed;
        updateInterruptibleStateLocked();
        changed_.notify_all();
        alreadyAborted=true;
      }
    }
    if(alreadyAborted){processQueue();return;}
    Context childContext;
    {
      std::lock_guard lock(mutex_);
      childContext=context_;
      childContext.stop=tracked.childStop.get_token();
    }
    bool thisGadgetErrored=false;
    bool syntheticAdded=false;
    runGadgetUse(tracked.block,tracked.assistantMessage,childContext,services_,[&](const Update& update){
      std::lock_guard lock(mutex_);
      if(syntheticAdded)return;
      const std::string reason=abortReasonLocked(tracked);
      if(!reason.empty()&&!thisGadgetErrored) {
        messages.push_back(syntheticError(tracked,reason));
        syntheticAdded=true;
        return;
      }
      if(update.message && isErrorResult(*update.message)) {
        thisGadgetErrored=true;
        if(internal::stringField(tracked.block,"name")==services_.bashGadgetName) {
          hasErrored_=true;
          erroredDescription_=gadgetDescription(tracked);
          siblingStop_.request_stop();
        }
      }
      if(update.message) {
        if(internal::field(*update.message,"type")=="progress") {
          tracked.pendingProgress.push_back(*update.message);
          changed_.notify_all();
        } else messages.push_back(*update.message);
      }
      if(update.contextModifier)modifiers.push_back(*update.contextModifier);
      if(update.appStateModifier)appStateModifiers.push_back(*update.appStateModifier);
    });
    {
      std::lock_guard lock(mutex_);
      tracked.results=std::move(messages);
      tracked.contextModifiers=std::move(modifiers);
      tracked.appStateModifiers=std::move(appStateModifiers);
      tracked.status=Status::Completed;
      updateInterruptibleStateLocked();
      if(!tracked.concurrencySafe)for(const auto& modifier:tracked.contextModifiers)
        context_.data=modifier(context_.data);
      if(!tracked.concurrencySafe)for(const auto& modifier:tracked.appStateModifiers)
        appendAppStateModifier(context_,modifier);
      changed_.notify_all();
    }
    processQueue();
  }catch(const std::exception& e) {
    std::lock_guard lock(mutex_);
    (void)e;
    failure_=std::current_exception();
    changed_.notify_all();
  }catch(...) {
    std::lock_guard lock(mutex_);
    failure_=std::current_exception();
    changed_.notify_all();
  }
}

std::vector<Update> StreamingGadgetExecutor::getCompletedResultsLocked(std::vector<std::string>& completedIDs) {
  std::vector<Update> output;
  if(discarded_)return output;
  for(auto& tracked:gadgets_) {
    while(!tracked->pendingProgress.empty()) {
      output.push_back(Update{tracked->pendingProgress.front(),{},tracked->id});
      tracked->pendingProgress.pop_front();
    }
    if(tracked->status==Status::Yielded)continue;
    if(tracked->status==Status::Completed) {
      tracked->status=Status::Yielded;
      for(const Json& message:tracked->results)output.push_back(Update{message,{},tracked->id});
      completedIDs.push_back(tracked->id);
    } else if(tracked->status==Status::Executing&&!tracked->concurrencySafe)break;
  }
  return output;
}

std::vector<Update> StreamingGadgetExecutor::getCompletedResults() {
  std::vector<std::string> ids;
  std::vector<Update> result;
  {
    std::lock_guard lock(mutex_);
    if(failure_)std::rethrow_exception(failure_);
    result=getCompletedResultsLocked(ids);
  }
  for(const auto& id:ids)if(services_.markGadgetUseComplete)services_.markGadgetUseComplete(id);
  return result;
}

void StreamingGadgetExecutor::getRemainingResults(const std::function<void(const Update&)>& yield) {
  while(true) {
    processQueue();
    auto ready=getCompletedResults();
    for(const auto& update:ready)yield(update);
    std::unique_lock lock(mutex_);
    if(discarded_)return;
    if(failure_)std::rethrow_exception(failure_);
    if(!unfinishedLocked())break;
    if(ready.empty())changed_.wait_for(lock,std::chrono::milliseconds(20));
  }
  for(const auto& update:getCompletedResults())yield(update);
}

Context StreamingGadgetExecutor::getUpdatedContext() const {
  std::lock_guard lock(mutex_);
  return context_;
}
} // namespace batbox::execution

#include "interactive/submit/CommandQueue.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace batbox::interactive::submit {
namespace {
const Json& field(const CommandRef& command,const char* name){
  return value::property(*command,name);
}
std::optional<std::string> content(const CommandRef& command){
  if(const auto& value=field(command,"value");value.is_string())
    return value.get<std::string>();
  return {};
}
}
CommandQueue::CommandQueue(event_loop::Loop& owner,QueueServices services):
    owner_(owner),services_(std::move(services)){}
std::function<void()> CommandQueue::subscribe(
    std::function<void()> listener){
  return changed_.subscribe(std::move(listener));
}
void CommandQueue::notify(){
  snapshot_=std::make_shared<const CommandList>(queue_);
  changed_.emit();
}
void CommandQueue::log(const std::string& operation,
                       const std::optional<std::string>& value){
  if(services_.logOperation)services_.logOperation(operation,value);
}
void CommandQueue::recheck(){if(!queue_.empty())notify();}
int CommandQueue::priority(const CommandRef& command){
  const auto& value=field(command,"priority");
  if(!value.is_string()||value=="next")return 1;
  if(value=="now")return 0;
  if(value=="later")return 2;
  return std::numeric_limits<int>::max();
}
void CommandQueue::enqueue(const Json& command){
  if(!owner_.isOwnerThread())throw std::logic_error("Queue owner thread required");
  if(!command.is_object())throw std::invalid_argument("Queued command object required");
  auto added=std::make_shared<Json>(command);
  if(!added->contains("priority")||added->at("priority").is_null())
    (*added)["priority"]="next";
  queue_.push_back(added);notify();log("enqueue",content(added));
}
void CommandQueue::enqueuePendingNotification(const Json& command){
  if(!command.is_object())throw std::invalid_argument("Queued command object required");
  Json added=command;
  if(!added.contains("priority")||added.at("priority").is_null())
    added["priority"]="later";
  enqueue(added);
}
CommandRef CommandQueue::peek(CommandPredicate filter)const{
  if(queue_.empty())return {};
  CommandRef best;
  int bestPriority=std::numeric_limits<int>::max();
  for(const auto& command:queue_){
    if(filter&&!filter(command))continue;
    const auto value=priority(command);
    if(value<bestPriority){best=command;bestPriority=value;}
  }
  return best;
}
CommandRef CommandQueue::dequeue(CommandPredicate filter){
  if(!owner_.isOwnerThread())throw std::logic_error("Queue owner thread required");
  auto best=peek(std::move(filter));
  if(!best)return {};
  const auto it=std::find(queue_.begin(),queue_.end(),best);
  queue_.erase(it);notify();log("dequeue");return best;
}
CommandList CommandQueue::dequeueAll(){
  if(!owner_.isOwnerThread())throw std::logic_error("Queue owner thread required");
  if(queue_.empty())return {};
  auto result=queue_;queue_.clear();notify();
  for(const auto& command:result){(void)command;log("dequeue");}
  return result;
}
CommandList CommandQueue::dequeueAllMatching(CommandPredicate predicate){
  if(!owner_.isOwnerThread())throw std::logic_error("Queue owner thread required");
  if(!predicate)throw std::invalid_argument("Queue predicate required");
  CommandList matched,remaining;
  for(const auto& command:queue_)
    (predicate(command)?matched:remaining).push_back(command);
  if(matched.empty())return {};
  queue_=std::move(remaining);notify();
  for(const auto& command:matched){(void)command;log("dequeue");}
  return matched;
}
void CommandQueue::remove(const CommandList& commands){
  if(!owner_.isOwnerThread())throw std::logic_error("Queue owner thread required");
  if(commands.empty())return;
  const auto before=queue_.size();
  queue_.erase(std::remove_if(queue_.begin(),queue_.end(),[&](const CommandRef& item){
    return std::find(commands.begin(),commands.end(),item)!=commands.end();
  }),queue_.end());
  if(queue_.size()!=before)notify();
  for(const auto& command:commands){(void)command;log("remove");}
}
CommandList CommandQueue::removeByFilter(CommandPredicate predicate){
  if(!owner_.isOwnerThread())throw std::logic_error("Queue owner thread required");
  if(!predicate)throw std::invalid_argument("Queue predicate required");
  CommandList removed,remaining;
  for(const auto& item:queue_)
    (predicate(item)?removed:remaining).push_back(item);
  if(removed.empty())return {};
  queue_=std::move(remaining);notify();
  for(const auto& command:removed){(void)command;log("remove");}
  return removed;
}
void CommandQueue::clear(){
  if(!owner_.isOwnerThread())throw std::logic_error("Queue owner thread required");
  if(queue_.empty())return;
  queue_.clear();notify();
}
void CommandQueue::reset(){
  if(!owner_.isOwnerThread())throw std::logic_error("Queue owner thread required");
  queue_.clear();snapshot_=std::make_shared<const CommandList>();
}
CommandList CommandQueue::getByMaxPriority(const std::string& name)const{
  auto target=std::make_shared<Json>(Json{{"priority",name}});
  const auto threshold=priority(target);
  CommandList result;
  for(const auto& item:queue_)if(priority(item)<=threshold)result.push_back(item);
  return result;
}
bool CommandQueue::isPoetryInputModeEditable(const std::string& mode){
  return mode!="task-notification";
}
bool CommandQueue::isQueuedCommandEditable(const CommandRef& command){
  const auto& mode=field(command,"mode");
  return isPoetryInputModeEditable(mode.is_string()?mode.get<std::string>():"")&&
    !value::truthy(field(command,"isMeta"));
}
bool CommandQueue::isQueuedCommandVisible(const CommandRef& command,
                                          bool channelsEnabled){
  if(channelsEnabled&&value::property(field(command,"origin"),"kind")=="channel")
    return true;
  return isQueuedCommandEditable(command);
}
bool CommandQueue::isSlashCommand(const CommandRef& command){
  const auto& value=field(command,"value");
  return value.is_string()&&value::trim(value.get<std::string>()).starts_with('/')&&
    !value::truthy(field(command,"skipSlashCommands"));
}
} // namespace batbox::interactive::submit

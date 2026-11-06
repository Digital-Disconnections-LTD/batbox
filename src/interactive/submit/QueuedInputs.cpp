#include "interactive/submit/QueuedInputs.hpp"
#include "messages/TextContent.hpp"
#include <algorithm>
#include <cstdio>

namespace batbox::interactive::submit {
namespace {
const Json& field(const Json& value,const char* name){
  return value::property(value,name);
}
std::string text(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_array())return messages::extractTextContent(value,"\n");
  return {};
}
// The id a delivered message carries for the queued command it came from.
const Json* deliveredId(const Json& message){
  const auto& type=field(message,"type");
  if(type=="user")return &field(message,"uuid");
  if(type!="attachment")return nullptr;
  const auto& attachment=field(message,"attachment");
  if(field(attachment,"type")!="queued_command")return nullptr;
  return &field(attachment,"source_uuid");
}
}
std::string queuedCommandId(const CommandRef& command){
  if(!command)return {};
  const auto& uuid=field(*command,"uuid");
  if(uuid.is_string()&&!uuid.get<std::string>().empty())return uuid.get<std::string>();
  char address[2*sizeof(void*)+16];
  std::snprintf(address,sizeof address,"queued:%p",static_cast<const void*>(command.get()));
  return address;
}
std::optional<QueuedInput> describeQueuedCommand(const CommandRef& command){
  if(!command||!command->is_object()||command->contains("agentId"))return {};
  if(!CommandQueue::isQueuedCommandEditable(command))return {};
  const auto& mode=field(*command,"mode");
  const bool shell=mode=="bash";
  if(!shell&&mode!="prompt")return {};
  QueuedInput row;
  row.id=queuedCommandId(command);
  row.text=text(field(*command,"preExpansionValue"));
  if(row.text.empty())row.text=text(field(*command,"value"));
  row.text=value::trim(row.text);
  if(row.text.empty())return {};
  row.kind=shell?QueuedInput::Kind::Shell:
    CommandQueue::isSlashCommand(command)?QueuedInput::Kind::Command:
    QueuedInput::Kind::Poetry;
  return row;
}
bool QueuedInputTracker::syncQueue(const CommandList& queue,
    const std::vector<MessageRef>& messages){
  std::vector<QueuedInput> queued;
  std::unordered_set<std::string> present;
  for(const auto& command:queue){
    auto row=describeQueuedCommand(command);
    if(!row)continue;
    present.insert(row->id);
    if(!delivered_.contains(row->id))queued.push_back(std::move(*row));
  }
  std::vector<QueuedInput> next;
  for(auto& row:rows_){
    if(present.contains(row.id))continue;
    if(withdrawn_.contains(row.id))continue;
    // Commands and ! runs leave their own rows once they start.
    if(row.kind!=QueuedInput::Kind::Poetry)continue;
    auto delivering=row;delivering.delivering=true;
    next.push_back(std::move(delivering));
  }
  std::erase_if(withdrawn_,[&](const std::string& id){return !present.contains(id);});
  std::erase_if(delivered_,[&](const std::string& id){return !present.contains(id);});
  if(rows_.empty())scanFrom_=messages.size();
  for(auto& row:queued)next.push_back(std::move(row));
  const auto same=[](const QueuedInput& left,const QueuedInput& right){
    return left.id==right.id&&left.text==right.text&&left.kind==right.kind&&
      left.delivering==right.delivering;
  };
  const bool changed=next.size()!=rows_.size()||
    !std::equal(next.begin(),next.end(),rows_.begin(),same);
  rows_=std::move(next);
  return dropDelivered(messages)||changed;
}
bool QueuedInputTracker::syncMessages(const std::vector<MessageRef>& messages){
  return dropDelivered(messages);
}
bool QueuedInputTracker::settle(){
  const auto before=rows_.size();
  std::erase_if(rows_,[](const QueuedInput& row){return row.delivering;});
  return rows_.size()!=before;
}
void QueuedInputTracker::withdraw(const CommandList& commands){
  for(const auto& command:commands)
    if(command)withdrawn_.insert(queuedCommandId(command));
}
bool QueuedInputTracker::dropDelivered(const std::vector<MessageRef>& messages){
  if(rows_.empty())return false;
  if(scanFrom_>messages.size())scanFrom_=0;
  std::unordered_set<std::string> found;
  for(auto index=scanFrom_;index<messages.size();++index){
    const auto& message=messages[index];
    if(!message)continue;
    const auto* id=deliveredId(*message);
    if(!id||!id->is_string())continue;
    const auto& value=id->get_ref<const std::string&>();
    if(std::any_of(rows_.begin(),rows_.end(),
        [&](const QueuedInput& row){return row.id==value;}))found.insert(value);
  }
  if(found.empty())return false;
  std::erase_if(rows_,[&](const QueuedInput& row){
    if(!found.contains(row.id))return false;
    if(!row.delivering)delivered_.insert(row.id);
    return true;
  });
  return true;
}
std::optional<PopEditableResult> takeQueuedPoetry(CommandQueue& queue,
    QueuedInputTracker& tracker,bool newestOnly,const std::string& input,
    std::size_t caret,double nowMs){
  // Only what the input drew can come back to it.
  const auto shown=[](const CommandRef& command){
    return describeQueuedCommand(command).has_value();
  };
  CommandList taken;
  for(const auto& command:queue.get())if(shown(command))taken.push_back(command);
  if(taken.empty())return {};
  if(newestOnly)taken.erase(taken.begin(),taken.end()-1);
  tracker.withdraw(taken);
  return queue.popEditable(shown,newestOnly,input,caret,nowMs);
}
} // namespace batbox::interactive::submit

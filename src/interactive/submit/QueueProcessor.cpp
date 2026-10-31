#include "interactive/submit/CommandQueue.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::interactive::submit {
namespace {
bool mainThread(const CommandRef& command){
  return !command->contains("agentId")||
    command->at("agentId").is_discarded();
}
bool slashForProcessor(const CommandRef& command){
  const auto& value=value::property(*command,"value");
  if(value.is_string())return value::trim(value.get<std::string>()).starts_with('/');
  if(!value.is_array())return false;
  for(const auto& block:value){
    if(value::property(block,"type")!="text")continue;
    const auto& text=value::property(block,"text");
    return text.is_string()&&
      value::trim(text.get<std::string>()).starts_with('/');
  }
  return false;
}
}
ProcessQueueResult processQueueIfReady(CommandQueue& queue,
                                       const ExecuteInput& executeInput){
  if(!executeInput)throw std::invalid_argument("Queue input executor required");
  const auto next=queue.peek(mainThread);
  if(!next)return {false};
  if(slashForProcessor(next)||value::property(*next,"mode")=="bash"){
    const auto command=queue.dequeue(mainThread);
    executeInput({command});return {true};
  }
  const auto mode=value::property(*next,"mode");
  const auto commands=queue.dequeueAllMatching([&](const CommandRef& command){
    return mainThread(command)&&!slashForProcessor(command)&&
      value::property(*command,"mode")==mode;
  });
  if(commands.empty())return {false};
  executeInput(commands);
  return {true};
}
bool hasQueuedCommands(const CommandQueue& queue){return !queue.empty();}
} // namespace batbox::interactive::submit

#include "user_input/SlashInternal.hpp"
#include "messages/Context.hpp"
#include "user_input/CommandMessages.hpp"
#include <stdexcept>

namespace batbox::user_input::internal {
std::string nameOf(const CommandEntry& entry){
  return entry.userFacingName?entry.userFacingName():
    commands::getCommandName(entry.descriptor);
}
const CommandEntry* findEntry(const std::string& name,
  const std::vector<CommandEntry>& entries){
  for(const auto& entry:entries){
    const auto& command=entry.descriptor;
    if(value::property(command,"name")==name||nameOf(entry)==name)
      return &entry;
    const auto& aliases=value::property(command,"aliases");
    if(aliases.is_array())for(const auto& alias:aliases)if(alias==name)return &entry;
  }
  return nullptr;
}
Json userMessage(const Json& content,const Json& preceding,
  messages::Services& services,const std::optional<std::string>& uuid){
  Json args={{"content",messages::prepareUserContent(
    Json{{"inputString",content},{"precedingInputBlocks",preceding}})}};
  if(uuid)args["uuid"]=*uuid;
  return messages::createUserMessage(args,services);
}
std::string errorText(const std::exception_ptr& error){
  if(!error)return "";
  try{std::rethrow_exception(error);}catch(const std::exception& e){
    return std::string("Error: ")+e.what();
  }catch(...){return "Error";}
}
std::string field(const Json& object,const char* key){
  const auto& value=value::property(object,key);
  return value.is_string()?value.get<std::string>():std::string{};
}
void require(bool present,const char* binding){
  if(!present)throw std::invalid_argument(std::string("Missing command binding: ")+binding);
}
InputResult finalizeCommandResult(InputResult value,messages::Services& services){
  if(value.messages.empty()){
    value.shouldQuery=false;
    value.allowedGadgets.reset();value.effort.reset();value.resultText.reset();
    return value;
  }
  bool allSystem=true;
  for(const auto& message:value.messages)
    if(!isSystemLocalCommandMessage(message)){allSystem=false;break;}
  if(!value.shouldQuery&&!allSystem&&
     !messages::isCompactBoundaryMessage(value.messages[0])){
    Json prefixed=Json::array({createSyntheticUserCaveatMessage(services)});
    for(const auto& message:value.messages)prefixed.push_back(message);
    value.messages=std::move(prefixed);
  }
  return value;
}
} // namespace batbox::user_input::internal

#pragma once
#include "user_input/SlashDispatch.hpp"

namespace batbox::user_input::internal {
const CommandEntry* findEntry(const std::string& name,
  const std::vector<CommandEntry>& commands);
std::string nameOf(const CommandEntry& entry);
Json userMessage(const Json& content,const Json& preceding,
  messages::Services& services,const std::optional<std::string>& uuid=std::nullopt);
std::string errorText(const std::exception_ptr& error);
std::string field(const Json& object,const char* key);
void require(bool present,const char* binding);
InputResult finalizeCommandResult(InputResult value,messages::Services& services);
event_loop::Deferred<InputResult> dispatchKnown(const CommandEntry& entry,
  const std::string& name,const std::string& args,const Json& preceding,
  const Json& images,const std::optional<std::string>& uuid,SlashServices& services);
} // namespace batbox::user_input::internal

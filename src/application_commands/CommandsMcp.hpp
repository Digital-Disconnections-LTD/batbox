#pragma once
#include "application_commands/Commands.hpp"

namespace batbox::application_commands {
user_input::CommandEntry makeMcpPoetryCommand(
  const std::shared_ptr<mcp::manager::IndigenousRuntime>& runtime,
  const mcp::ResultServices& results,
  const std::function<event_loop::Deferred<Json>(const std::string&)>& getAttachments,
  const std::shared_ptr<messages::Services>& messages,
  const std::shared_ptr<bootstrap::InvokedSkillsState>& invokedSkills,
  const std::function<std::shared_ptr<bats::BatContext>()>& currentBat,
  event_loop::Loop& owner,const Json& descriptor);
user_input::CommandEntry makeMcpCommand(
  const std::shared_ptr<user_input::UiCommandRouter>& router,
  const std::shared_ptr<mcp::manager::IndigenousRuntime>& runtime,
  const std::shared_ptr<McpAuthUi>& auth);
} // namespace batbox::application_commands

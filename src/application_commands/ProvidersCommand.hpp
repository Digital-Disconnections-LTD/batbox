#pragma once
#include "application/Config.hpp"
#include "application_model/LocalModelService.hpp"
#include "app_state/State.hpp"
#include "event_loop/Loop.hpp"
#include "interactive/CommandUi.hpp"
#include "providers/LocalDiscovery.hpp"
#include "providers/Onboarding.hpp"
#include "user_input/UiCommandRouter.hpp"
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace batbox::application_commands {
struct ProvidersInputs {
  std::filesystem::path configPath,legacyPath;
  // Null in tests; the live session gets the new profile when present.
  std::shared_ptr<application_model::LocalModelService> model;
  std::shared_ptr<app_state::AppStateStore> store;
  bool interactive=true;
  // The profile in use, for the "active" badge.
  std::function<std::string()> activeProfile;
  // These run on a worker. Defaults: LocalDiscovery, testConnection,
  // EndpointStore reads and providers::saveEndpoint.
  std::function<std::vector<providers::LocalServer>()> discover;
  std::function<providers::ConnectionResult(const application::EndpointRecord&)> test;
  std::function<std::map<std::string,application::EndpointRecord>()> readEndpoints;
  std::function<providers::SaveResult(std::map<std::string,application::EndpointRecord>,
    const std::string& id,application::EndpointRecord endpoint,const std::string& model)> save;
  providers::Environment environment;
  const providers::Catalog* catalog=nullptr;
  // Redraw the connection spinner; golden frames turn this off.
  bool animate=true;
  // Called on the owner after a provider is saved.
  std::function<void(const providers::SaveResult&)> onSaved;
};
// Fills unset services with the live defaults.
std::shared_ptr<ProvidersInputs> completeProvidersInputs(ProvidersInputs inputs);
// Runs the dialog with explicit callbacks; exposed for golden frames.
void startProvidersDialog(event_loop::Loop& owner,std::shared_ptr<ProvidersInputs> inputs,
  interactive::CommandUi::Done done,interactive::CommandUi::Ready ready,std::string notice={});
// Plain text for --print: local servers, configured endpoints and the hint.
std::string providersText(const std::vector<providers::LocalServer>& servers,
  const std::map<std::string,application::EndpointRecord>& endpoints);
// /providers walks a person from a provider list to a saved endpoint.
user_input::CommandEntry makeProvidersCommand(event_loop::Loop& owner,
  std::shared_ptr<user_input::UiCommandRouter> router,ProvidersInputs inputs);
} // namespace batbox::application_commands

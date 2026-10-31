#pragma once
#include "application_engine/Compactor.hpp"
#include "application_engine/Engine.hpp"
#include "application_hooks/Provider.hpp"
#include "application_poetry/Provider.hpp"
#include "application_flights/Flight.hpp"
#include "application_permissions/PermissionProvider.hpp"
#include "bootstrap/InvokedSkills.hpp"
#include "gadgets/files/FileRead.hpp"

namespace batbox::application_engine {
struct CompactBindingInputs {
  event_loop::Loop& owner;
  application::Config config;
  std::shared_ptr<application_model::LocalModelService> model;
  std::shared_ptr<messages::Services> messages;
  std::shared_ptr<application_hooks::Provider> hooks;
  std::shared_ptr<application_poetry::Provider> poetry;
  std::shared_ptr<application_flights::Flight> flight;
  std::shared_ptr<app_state::AppStateStore> store;
  std::shared_ptr<application_permissions::PermissionProvider> permission;
  std::function<std::shared_ptr<Engine>()> currentEngine;
  query::NotifyLifecycle notifyCommandLifecycle;
  std::function<gadgets::files::FileReadServices()> readServices;
  AttachmentNormalizer normalizeAttachment;
  std::shared_ptr<bootstrap::InvokedSkillsState> invokedSkills;
  std::function<void(std::vector<std::string>)> updateWatchPaths;
  std::function<void(const Json&)> progress,notification;
  std::function<void(const std::string&)> log;
  explicit CompactBindingInputs(event_loop::Loop& loop):owner(loop){}
};
std::shared_ptr<Compactor> makeCompactor(CompactBindingInputs inputs);
void bindCompactor(Inputs& engine,std::shared_ptr<Compactor> compactor);
} // namespace batbox::application_engine

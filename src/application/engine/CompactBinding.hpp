#pragma once
#include "application/engine/Compactor.hpp"
#include "application/engine/Engine.hpp"
#include "application/hooks/Provider.hpp"
#include "application/poetry/Provider.hpp"
#include "application/flights/Flight.hpp"
#include "application/permissions/PermissionProvider.hpp"
#include "bootstrap/InvokedSkills.hpp"
#include "gadgets/files/FileRead.hpp"

namespace batbox::application::engine {
struct CompactBindingInputs {
  event_loop::Loop& owner;
  application::Config config;
  std::shared_ptr<application::model::LocalModelService> model;
  std::shared_ptr<messages::Services> messages;
  std::shared_ptr<application::hooks::Provider> hooks;
  std::shared_ptr<application::poetry::Provider> poetry;
  std::shared_ptr<application::flights::Flight> flight;
  std::shared_ptr<app_state::AppStateStore> store;
  std::shared_ptr<application::permissions::PermissionProvider> permission;
  std::function<std::shared_ptr<Engine>()> currentEngine;
  query::NotifyLifecycle notifyCommandLifecycle;
  std::function<batbox::gadgets::files::FileReadServices()> readServices;
  AttachmentNormalizer normalizeAttachment;
  std::shared_ptr<bootstrap::InvokedSkillsState> invokedSkills;
  std::function<void(std::vector<std::string>)> updateWatchPaths;
  std::function<void(const Json&)> progress,notification;
  std::function<void(const std::string&)> log;
  explicit CompactBindingInputs(event_loop::Loop& loop):owner(loop){}
};
std::shared_ptr<Compactor> makeCompactor(CompactBindingInputs inputs);
void bindCompactor(Inputs& engine,std::shared_ptr<Compactor> compactor);
} // namespace batbox::application::engine

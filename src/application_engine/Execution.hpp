#pragma once
#include "bats/ForkedBat.hpp"
#include "event_loop/Loop.hpp"
#include "messages/Messages.hpp"
#include "gadget_results/Storage.hpp"

namespace batbox::application_engine {
struct ExecutionInputs {
  event_loop::Loop& owner;
  std::shared_ptr<messages::Services> messages;
  gadget_results::StorageImports storage;
  // Hook/permission imports are bound by their application owners. This
  // factory supplies indigenous message, state, persistence and error boundaries.
  execution::Services imports;
  std::function<Json()> getAppState;
  std::function<void(const std::function<Json(const Json&)>&)> updateAppState;
  std::function<std::shared_ptr<bats::BatContext>()> currentBatContext;
  std::function<void(const std::string&,bool)> gadgetProgress;
  std::function<void(double)> gadgetDuration;
  bool autoMemoryEnabled=false,memoryCorrectionHint=false;
  explicit ExecutionInputs(event_loop::Loop& loop):owner(loop){}
};
std::shared_ptr<execution::Services> makeExecutionServices(ExecutionInputs inputs);
} // namespace batbox::application_engine

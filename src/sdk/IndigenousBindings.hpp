#pragma once
#include "sdk/EventQueue.hpp"
#include "flights/Lifecycle.hpp"
#include "bats/BatMissionRegistration.hpp"
#include "bats/BatGadgetForegroundFinalize.hpp"
namespace batbox::sdk {
QueueServices indigenousQueueServices(std::function<bool()> nonInteractive,
  std::function<std::string()> flightId);
flights::LifecycleServices lifecycleQueueServices(std::shared_ptr<EventQueue> queue,
  std::function<std::optional<std::string>(const std::string&)> environment={});
void bindBatSdkQueue(bats::BatRegistrationServices& services,std::shared_ptr<EventQueue> queue);
void bindForegroundSdkQueue(bats::BatGadgetForegroundFinalServices& services,std::shared_ptr<EventQueue> queue);
}

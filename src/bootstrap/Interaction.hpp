#pragma once
#include "event_loop/Deferred.hpp"
#include <functional>
#include <memory>
#include <optional>

namespace batbox::bootstrap {
struct InteractionTimer {
  std::function<void()> cancel,unref;
};
struct InteractionServices {
  std::function<double()> nowMs;
  std::function<InteractionTimer(double,std::function<void()>)> setTimeout;
};
struct InteractionState {
  double lastInteractionTime=0;
  bool interactionTimeDirty=false,scrollDraining=false;
  std::optional<InteractionTimer> scrollDrainTimer;
};
std::shared_ptr<InteractionState> makeInteractionState(
  const InteractionServices& services);
void updateLastInteractionTime(const std::shared_ptr<InteractionState>& state,
  const InteractionServices& services,bool immediate=false);
void flushInteractionTime(const std::shared_ptr<InteractionState>& state,
  const InteractionServices& services);
double getLastInteractionTime(const std::shared_ptr<InteractionState>& state);
void markScrollActivity(const std::shared_ptr<InteractionState>& state,
  const InteractionServices& services);
bool getIsScrollDraining(const std::shared_ptr<InteractionState>& state);
event_loop::Deferred<event_loop::Unit> waitForScrollIdle(event_loop::Loop& loop,
  const std::shared_ptr<InteractionState>& state,
  const InteractionServices& services);
InteractionServices makeIndigenousInteractionServices(event_loop::Loop& loop);
}

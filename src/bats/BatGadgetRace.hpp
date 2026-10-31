#pragma once
#include "bats/BatGadgetStream.hpp"
#include "bats/BatMissionForeground.hpp"
#include <future>
#include <optional>

namespace batbox::bats {
struct BatGadgetRaceResult {
  bool background=false;
  std::optional<query::MessageRef> message;
};
// Race adapter: message mapping is registered inside this call; background
// mapping was registered at mission creation. Producer notifications carry
// their reaction-enqueue order directly.
BatGadgetRaceResult raceBatMessageAndBackground(
  ObservedBatMessage nextMessage,
  const std::optional<ObservedBackgroundSignal>& backgroundSignal);

// Legacy shared_future bridge retained for isolated callers without producer
// observation. It samples readiness and is not used by BatGadgetForeground.
BatGadgetRaceResult raceBatMessageAndBackground(
  std::shared_future<std::optional<query::MessageRef>> nextMessage,
  const std::optional<std::shared_future<void>>& backgroundSignal);
} // namespace batbox::bats

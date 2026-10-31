#pragma once
#include "app_state/State.hpp"

namespace batbox::app_state {
value::ValueRef getViewedPartnerMission(const AppState& state);
struct ActiveBatForInput {
  enum class Type { Leader,Viewed,NamedBat } type=Type::Leader;
  value::ValueRef mission;
  Json snapshot()const;
};
ActiveBatForInput getActiveBatForInput(const AppState& state);
} // namespace batbox::app_state

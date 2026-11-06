#pragma once
#include <optional>
#include <string>

namespace batbox::bootstrap {
struct PoetryState {
  std::optional<std::string> lastEmittedDate;
};
const std::optional<std::string>& getLastEmittedDate(const PoetryState& state);
void setLastEmittedDate(PoetryState& state,std::optional<std::string> date);
}

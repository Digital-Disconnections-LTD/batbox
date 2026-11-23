#pragma once
#include <optional>
#include <string>
#include <utility>

namespace batbox::bootstrap {
struct PoetryState {
  std::optional<std::string> lastEmittedDate;
};
inline const std::optional<std::string>& getLastEmittedDate(const PoetryState& state){
  return state.lastEmittedDate;
}
inline void setLastEmittedDate(PoetryState& state,std::optional<std::string> date){
  state.lastEmittedDate=std::move(date);
}
}

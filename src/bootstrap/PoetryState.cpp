#include "bootstrap/PoetryState.hpp"
#include <utility>

namespace batbox::bootstrap {
const std::optional<std::string>& getLastEmittedDate(const PoetryState& state){
  return state.lastEmittedDate;
}
void setLastEmittedDate(PoetryState& state,std::optional<std::string> date){
  state.lastEmittedDate=std::move(date);
}
}

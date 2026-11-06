#pragma once
#include "cwd/State.hpp"
#include <string>

namespace batbox::bootstrap {
struct IdentityState {
  cwd::State directories;
  std::string flightId;
};
}

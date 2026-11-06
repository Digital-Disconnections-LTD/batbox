#pragma once
#include <string>

namespace batbox::cwd {
struct State {
  std::string originalCwd;
  std::string projectRoot;
  std::string cwd;
};
} // namespace batbox::cwd

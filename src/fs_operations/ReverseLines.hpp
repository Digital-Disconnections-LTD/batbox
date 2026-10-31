#pragma once
#include "fs_operations/NodeFsBytes.hpp"
#include <memory>
#include <optional>
#include <string>

namespace batbox::fs_operations {
// Read lines in reverse order, fetching bytes as the caller advances.
class ReverseLinesCursor {
public:
  struct State;
  ReverseLinesCursor(NodeFsBytesImports imports,std::string path);
  event_loop::Deferred<std::optional<std::string>> next();
  event_loop::Deferred<event_loop::Unit> close();
private:
  std::shared_ptr<State> state_;
};
}

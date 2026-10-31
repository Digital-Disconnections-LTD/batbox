#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace batbox::diagnostics {
using ExecSyncResult=std::variant<std::string,std::vector<unsigned char>>;
struct ExecSyncImports {
  std::function<std::function<void()>(const std::vector<std::string>&,
    const std::vector<std::optional<Json>>&)> beginSlowLogging;
  std::function<ExecSyncResult(const std::string&,const std::optional<Json>&)> nodeExecSync;
};

ExecSyncResult execSyncDeprecated(const std::string& command,
  std::optional<Json> options,const ExecSyncImports& imports);
}

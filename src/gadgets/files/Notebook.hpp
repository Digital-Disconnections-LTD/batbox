#pragma once
#include "filesystem/FileOps.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::gadgets::files::notebook {
struct Services {
  filesystem::PathServices paths;
  std::function<std::string(const std::string&)> formatOutput;
  std::string bashGadgetName="Bash";
};
Json readNotebook(const std::string& notebookPath,const Services& services,
                  const std::optional<std::string>& cellId=std::nullopt);
Json mapNotebookCellsToGadgetResult(const Json& cells,const std::string& gadgetUseID);
std::optional<double> parseCellId(const std::string& cellId);
} // namespace batbox::gadgets::files::notebook

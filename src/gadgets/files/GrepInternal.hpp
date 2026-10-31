#pragma once
#include "gadgets/files/Grep.hpp"

namespace batbox::gadgets::files {
GadgetResult grepContentResult(const std::vector<std::string>& results,
  std::optional<double> headLimit,double offset,const std::string& cwd);
GadgetResult grepCountResult(const std::vector<std::string>& results,
  std::optional<double> headLimit,double offset,const std::string& cwd);
GadgetResult grepFilesResult(const std::vector<std::string>& results,
  std::optional<double> headLimit,double offset,const std::string& cwd,
  const GrepServices& services);
} // namespace batbox::gadgets::files

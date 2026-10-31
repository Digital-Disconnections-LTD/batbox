#pragma once
#include <functional>
#include <optional>
#include <string>

namespace batbox::model_stash {

struct DiffWriterServices {
  std::function<std::string()> tempDir;
  std::function<double()> random;
};

// Write a local diff for debugging stash behavior.
// Poetry content is not sent to a remote service.
std::optional<std::string> writeStashBreakDiff(const std::string& before,
    const std::string& after,const DiffWriterServices& services);

} // namespace batbox::model_stash

#pragma once
#include "gadgets/bash/BashGadget.hpp"
#include <cstdint>

namespace batbox::bash_gadget {
struct BashPreparedResult {
  shell::ExecResult command;
  Json interpretation;
  std::string stdoutText,shellReset;
  // A failed command's output, raised as a ShellError only once a large
  // output has been saved, so the error can name the saved file.
  std::optional<std::string> failureOutput;
  bool mainThread=false;
};
struct BashPersistedOutput {
  std::string path;
  std::optional<std::uintmax_t> originalSize;
};
struct BashFinalInput {
  std::string cleanOutput;
  bool isImage=false;
};
// Interpret the synchronous Bash result and finalize the UI hint on both
// success and failure. A failed command is returned with failureOutput set;
// throwIfBashFailed raises it after persistence.
BashPreparedResult prepareBashCompletion(const Json& input,
  const gadgets::GadgetCallContext& context,shell::ExecResult result,
  const Services& services,const std::function<Json()>& getGadgetPermissionContext={});
// Indigenous filesystem binding for async stat, ensure, truncate, link and copy.
// The async call runs on an I/O worker and posts completion to the owner.
BashPersistedOutput persistBashOutput(const shell::ExecResult& result,
  const Services& services);
void throwIfBashFailed(const BashPreparedResult& prepared,
  const BashPersistedOutput& persisted);
BashFinalInput prepareBashFinalInput(const Json& input,
  const BashPreparedResult& prepared,const Services& services);
gadgets::GadgetResult finishBashFinalOutput(const Json& input,
  const BashPreparedResult& prepared,const BashPersistedOutput& persisted,
  const BashFinalInput& finalInput,const std::optional<std::string>& resized,
  const Services& services);
gadgets::GadgetResult finishBashCompletion(const Json& input,
  const BashPreparedResult& prepared,const BashPersistedOutput& persisted,
  const Services& services);
} // namespace batbox::bash_gadget

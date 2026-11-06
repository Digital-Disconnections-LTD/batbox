#pragma once
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace batbox::application {
// Work to do once when Batbox exits; steps may be added from any thread.
struct ExitSteps {
  std::mutex mutex;
  std::vector<std::function<void()>> steps;
};
void addExitStep(ExitSteps& exit,std::function<void()> step);
void runExitSteps(ExitSteps& exit);
// The BashProvider createSnapshot binding: the first call starts one snapshot
// of the user's shell (functions, aliases and options from its rc file) on a
// worker and every later call returns the same result. A failure yields no
// snapshot, so commands run as they do without one. The file lives under
// stateDirectory/shell-snapshots and is removed by the exit steps.
std::function<std::shared_future<std::optional<std::string>>(const std::string&)>
  makeShellSnapshot(std::filesystem::path stateDirectory,std::shared_ptr<ExitSteps> exit,
    std::function<void(const std::string&)> log);
}

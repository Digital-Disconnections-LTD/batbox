#pragma once
#include "terminal_renderer/Frame.hpp"
#include "termio/OscLink.hpp"
#include <functional>
#include <memory>

namespace batbox::terminal_log_update {
struct Options {
  bool isTTY=false;
  std::shared_ptr<terminal_screen::StylePool> stylePool;
};
struct Services {
  std::function<double()> performanceNow;
  std::function<void(const std::string&)> logForDebugging;
  std::shared_ptr<termio::OscModule> oscModule;
};
class LogUpdate {
public:
  LogUpdate(Options options,Services services);
  terminal_renderer::Diff renderPreviousOutput_DEPRECATED(
    const terminal_renderer::Frame& previous);
  void reset();
  terminal_renderer::Diff render(const terminal_renderer::Frame& previous,
    const terminal_renderer::Frame& next,bool altScreen=false,
    bool decstbmSafe=true);
private:
  terminal_renderer::Diff renderFullFrame(
    const terminal_renderer::Frame& frame);
  terminal_renderer::Diff getRenderOpsForDone(
    const terminal_renderer::Frame& previous);
  Options options_;
  Services services_;
  std::string previousOutput_;
};
} // namespace batbox::terminal_log_update

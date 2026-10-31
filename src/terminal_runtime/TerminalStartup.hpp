#pragma once
#include "terminal_runtime/TerminalControls.hpp"
#include "terminal_runtime/ReactTree.hpp"
#include "terminal_render/RenderNode.hpp"
#include "terminal_dom/Dom.hpp"
#include <memory>

namespace batbox::terminal_runtime {
struct StartupOptions {
  std::shared_ptr<OutputPort> stdout,stderr;
  std::shared_ptr<InputPort> stdin;
  bool exitOnCtrlC=true,patchConsole=true;
  std::function<void(const terminal_renderer::FrameEvent&)> onFrame;
};
struct StartupServices {
  explicit StartupServices(event_loop::Loop& loop):ownerLoop(loop){}
  event_loop::Loop& ownerLoop;
  std::function<double()> performanceNow;
  std::function<void(const std::string&,const std::string&)>
    logForDebugging;
  terminal_screen::DebugWarning screenDebug;
  std::function<bool()> supportsExtendedKeys;
  std::function<void()> flushInteractionTime;
  ProcessLifecycleServices processLifecycle;
  std::shared_ptr<ConsolePort> console;
  ConsoleServices consoleServices;
  terminal_dom::DomServicesRef dom;
  std::shared_ptr<terminal_events::Dispatcher> dispatcher;
  terminal_events::MonotonicNow monotonicNow;
  terminal_render::RenderNodeImports renderNodeImports;
  terminal_output::OutputRenderServices outputRender;
  std::shared_ptr<const terminal_color::ChalkState> chalk;
  std::function<event_loop::Deferred<std::string>(const std::string&)>
    setClipboard;
  std::function<void(std::exception_ptr)> unhandledClipboardRejection;
  std::function<std::optional<std::string>()> debugRepaintsEnv;
  std::shared_ptr<termio::OscModule> oscModule;
  termio::OutputServices output;
  cleanup::TerminalSequences sequences;
  std::function<void(const std::string&)> writeSyncStdout;
  std::function<bool()> supportsTabStatus;
  std::function<std::string(const std::string&)> wrapForMultiplexer;
  std::function<void()> deleteInstance;
  ReactTreeServices react;
  bool syncOutputSupported=false;
};
// The returned controller retains all indigenous dependencies through unmount.
std::shared_ptr<TerminalControls> createTerminalControls(
  StartupOptions options,StartupServices services);
} // namespace batbox::terminal_runtime

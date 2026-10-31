#pragma once
#include "event_loop/Loop.hpp"
#include <ftxui/component/app.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/dom/node.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace batbox::terminal_ftxui {
enum class Mode {Inline,Fullscreen};
struct Options {
  Mode mode=Mode::Inline;
  bool interactive=true;
  int inputFd=0,outputFd=1;
  int redirectedWidth=80,redirectedHeight=24;
  double pumpIntervalMs=8;
};
struct CommittedRow {
  std::string id;
  // One display row, already styled and wrapped for the current terminal.
  std::string text;
};
struct Callbacks {
  std::function<ftxui::Element()> render;
  // Exact bytes from FTXUI's input read, before its parser normalizes events.
  // This is the sole ingress for the existing keyboard/query parser.
  std::function<void(std::string_view)> onRawInputBytes;
  // FTXUI events are optional metadata; do not feed them into the raw parser.
  std::function<void(const ftxui::Event&)> onEvent;
  std::function<void(ftxui::Dimensions)> onResize;
  // Redirected output stays plain and uses the application's incremental
  // output formatter. FTXUI never installs terminal modes for this path.
  std::function<std::string()> renderPlain;
  std::function<void(std::string_view)> writePlain;
  std::function<void()> onExit;
  // Runs after a rendered interactive frame has been written successfully.
  std::function<void()> onFrameCommitted;
  std::function<bool(const ftxui::Event&)> consumeMouseEvent={};
  std::function<void(const std::string&)> copySelectedText={};
};
class Driver:public std::enable_shared_from_this<Driver> {
public:
  Driver(event_loop::Loop& owner,Options options,Callbacks callbacks);
  ~Driver();
  Driver(const Driver&)=delete;
  Driver& operator=(const Driver&)=delete;
  void start();
  void stop();
  void setMode(Mode mode);
  void requestRedraw();
  void pumpOnce();
  // Send a nonvisual terminal control sequence through the FTXUI output owner.
  // Calls during a frame are drained after FTXUI flushes that frame.
  void writeRaw(std::string_view bytes);
  // Append durable rows above the live inline frame. IDs are accepted once
  // per driver lifetime; fullscreen retains rows in its own viewport.
  void commitScrollback(const std::vector<CommittedRow>& rows);
  // Uninstall terminal hooks around a synchronous external command/editor.
  // The callback may throw; FTXUI is reinstalled before the exception escapes.
  void withRestoredIO(const std::function<void()>& callback);
  // For an asynchronous external command, release the terminal completely and
  // restart the selected FTXUI mode when the command finishes.
  void suspend();
  void resume();
  bool running()const{return running_;}
  bool suspended()const{return suspended_;}
  bool interactive()const{return interactive_;}
  Mode mode()const{return options_.mode;}
  std::string selectedText()const;
private:
  class RootComponent;
  void startInteractive();
  void startRedirected();
  void schedulePump();
  void teardown(bool notifyExit);
  void checkResize();
  void drawRedirected();
  void flushRaw();
  void flushScrollback();
  event_loop::Loop& owner_;
  Options options_;
  Callbacks callbacks_;
  std::unique_ptr<ftxui::App> app_;
  std::unique_ptr<ftxui::Loop> loop_;
  ftxui::Component component_;
  event_loop::Loop::TimerHandle pumpTimer_;
  std::optional<ftxui::Dimensions> lastSize_;
  std::optional<Mode> pendingMode_;
  std::unordered_set<std::string> committedRowIds_;
  std::unordered_set<std::string> pendingRowIds_;
  std::vector<CommittedRow> pendingRows_;
  std::vector<std::string> pendingRaw_;
  bool running_=false,interactive_=false,inPump_=false,
    redrawAfterPump_=false,exitNotified_=false,suspended_=false,
    frameRendered_=false;
  bool selectionDragging_=false,selectionMoved_=false,copySelectionPending_=false;
};
} // namespace batbox::terminal_ftxui

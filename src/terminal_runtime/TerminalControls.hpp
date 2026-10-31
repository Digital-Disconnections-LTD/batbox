#pragma once
#include "terminal_renderer/Frame.hpp"
#include "terminal_selection/Selection.hpp"
#include "terminal_hit_test/HitTest.hpp"
#include "terminal_events/Dispatcher.hpp"
#include "terminal_events/KeyboardEvent.hpp"
#include "event_loop/Deferred.hpp"
#include "terminal_render/ScreenSearch.hpp"
#include "terminal_output/OutputRender.hpp"
#include "terminal_color/Colorize.hpp"
#include "terminal_log_update/LogUpdate.hpp"
#include "terminal_renderer/Renderer.hpp"
#include "terminal_render/RenderState.hpp"
#include "terminal_reconciler/HostLeaves.hpp"
#include "cleanup/Terminal.hpp"
#include "termio/Output.hpp"
#include "lodash_throttle/Throttle.hpp"
#include <functional>
#include <cstddef>
#include <cstdint>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <variant>

#if defined(_WIN32) && defined(stdin)
// Preserve stdin property names after the Windows CRT headers.
#undef stdin
#endif
#if defined(_WIN32) && defined(stdout)
#undef stdout
#endif
#if defined(_WIN32) && defined(stderr)
#undef stderr
#endif
namespace batbox::terminal_runtime {
struct ReactTreeServices;
struct ReactTreeState;
struct Listener {std::function<void()> callback;};
using ListenerRef=std::shared_ptr<Listener>;
struct InputPort {
  virtual ~InputPort()=default;
  virtual bool isTTY()const=0;
  virtual bool isRaw()const=0;
  virtual bool hasSetRawMode()const=0;
  virtual void setRawMode(bool)=0;
  virtual std::vector<ListenerRef> listeners(const std::string& event)const=0;
  virtual void addListener(const std::string& event,ListenerRef listener)=0;
  virtual void removeListener(const std::string& event,ListenerRef listener)=0;
  // std::nullopt projects Node's null read() result.
  virtual std::optional<std::string> read()=0;
};
struct OutputPort {
  virtual ~OutputPort()=default;
  virtual bool isTTY()const=0;
  virtual std::optional<double> columns()const=0;
  virtual std::optional<double> rows()const=0;
  virtual void write(const std::string& data)=0;
};
using ReactNodeRef=std::shared_ptr<void>;
struct DrainServices {
  bool windows=false;
  std::function<int()> openNonblockingTty;
  std::function<std::ptrdiff_t(int,char*,std::size_t)> readTty;
  std::function<void(int)> closeTty;
};
DrainServices indigenousDrainServices();
struct ControlServices {
  // React's flushSyncFromReconciler and render tree updates are required
  // services. The full React Fiber scheduler remains a separate SDK component.
  std::function<void()> flushSyncFromReconciler,onRender,resetLog;
  std::function<void(ReactNodeRef)> renderCurrentNode;
  std::function<bool()> supportsExtendedKeys;
  std::function<void(const std::string&,const std::string&)> logForDebugging;
  terminal_screen::DebugWarning screenDebug;
  DrainServices drain;
  // scheduleRender.cancel is optional.
  std::function<void()> cancelScheduledRender;
  event_loop::Loop* ownerLoop=nullptr;
};
struct CursorPoint {double x=0,y=0;};
struct CursorDeclaration {
  double relativeX=0,relativeY=0;
  terminal_dom::ElementRef node;
};
using CursorDeclarationRef=std::shared_ptr<CursorDeclaration>;
struct SelectionListener {std::function<void()> callback;};
using SelectionListenerRef=std::shared_ptr<SelectionListener>;
struct SearchPositions {
  std::vector<terminal_render::MatchPosition> positions;
  double rowOffset=0,currentIdx=0;
};
using SearchPositionsRef=std::shared_ptr<SearchPositions>;
struct InteractionServices {
  std::function<void()> scheduleRender;
  std::function<event_loop::Deferred<std::string>(const std::string&)> setClipboard;
  std::function<void(std::exception_ptr)> unhandledClipboardRejection;
  terminal_dom::ElementRef rootNode;
  terminal_dom::FocusManager* focusManager=nullptr;
  terminal_dom::NodeStash* nodeStash=nullptr;
  terminal_hit_test::PointerHandlerStore* pointerHandlers=nullptr;
  terminal_events::Dispatcher* dispatcher=nullptr;
  terminal_events::MonotonicNow monotonicNow;
  terminal_render::RenderNodeRenderer* renderNode=nullptr;
  const terminal_output::OutputRenderServices* outputRender=nullptr;
  terminal_render::ScanState* scanState=nullptr;
  const terminal_color::ChalkState* chalk=nullptr;
};
struct ShutdownServices {
  terminal_log_update::LogUpdate* log=nullptr;
  termio::OutputServices output;
  cleanup::TerminalSequences sequences;
  std::function<void(const std::string&)> writeSyncStdout;
  std::function<bool()> supportsTabStatus;
  std::function<std::string(const std::string&)> wrapForMultiplexer;
  std::function<void()> unsubscribeExit,restoreConsole,restoreStderr,
    unsubscribeTTYHandlers,updateContainerNull,flushSyncWork,
    deleteInstance;
  terminal_dom::ElementRef rootNode;
};
struct YogaFrameCounters {
  double ms=0,visited=0,measured=0,stashHits=0,live=0;
};
struct RenderCycleServices {
  std::function<void()> flushInteractionTime;
  std::function<double()> performanceNow;
  terminal_renderer::Renderer* renderer=nullptr;
  terminal_render::RenderState* renderState=nullptr;
  terminal_log_update::LogUpdate* log=nullptr;
  terminal_reconciler::ProfileCounters* profile=nullptr;
  terminal_reconciler::DebugRepaints* debugRepaints=nullptr;
  termio::OutputServices output;
  bool syncOutputSupported=false;
  std::function<void(const terminal_renderer::FrameEvent&)> onFrame;
};
struct OwnerRootServices {
  terminal_dom::ElementRef rootNode;
  terminal_dom::FocusManager* focusManager=nullptr;
  terminal_reconciler::ProfileCounters* profile=nullptr;
  std::function<double()> performanceNow;
  std::function<void()> scheduleRender;
};
using StderrChunk=std::variant<std::string,std::vector<std::uint8_t>>;
struct StderrWriteCall {
  StderrChunk chunk;
  std::optional<std::string> encoding;
  std::function<void()> callback;
};
using StderrWrite=std::function<bool(const StderrWriteCall&)>;
using StderrWriteRef=std::shared_ptr<StderrWrite>;
class StderrPort {
public:
  virtual ~StderrPort()=default;
  virtual StderrWriteRef writeMethod()const=0;
  virtual void setWriteMethod(StderrWriteRef method)=0;
};
using ConsoleArgRef=std::shared_ptr<void>;
using ConsoleArgs=std::vector<ConsoleArgRef>;
struct ConsoleMethod {std::function<void(const ConsoleArgs&)> invoke;};
using ConsoleMethodRef=std::shared_ptr<ConsoleMethod>;
class ConsolePort {
public:
  virtual ~ConsolePort()=default;
  virtual ConsoleMethodRef method(const std::string& name)const=0;
  virtual void setMethod(const std::string& name,ConsoleMethodRef method)=0;
};
struct ConsoleServices {
  std::function<std::string(const ConsoleArgs&)> format;
  std::function<bool(const ConsoleArgRef&)> truthy;
  std::function<void(std::exception_ptr)> logError;
};
struct ProcessLifecycleServices {
  std::function<std::function<void()>(ListenerRef,bool)> onExit;
  std::function<void(ListenerRef)> onStdoutResize,offStdoutResize,
    onSigcont,offSigcont;
};
struct StoredListener {std::string event;ListenerRef listener;};
class TerminalControls:public std::enable_shared_from_this<TerminalControls> {
public:
  TerminalControls(OutputPort& stdout,InputPort& stdin,
    ControlServices services);
  TerminalControls(OutputPort& stdout,InputPort& stdin,
    std::shared_ptr<terminal_screen::StylePool> stylePool,
    std::shared_ptr<terminal_screen::CharPool> charPool,
    std::shared_ptr<terminal_screen::HyperlinkPool> hyperlinkPool,
    ControlServices services);
  void pause();
  void resume();
  void repaint();
  void forceRedraw();
  void invalidatePrevFrame();
  void setAltScreenActive(bool active,bool mouseTracking=false);
  bool isAltScreenActive()const{return altScreenActive_;}
  void reassertTerminalModes(bool includeAltScreen=false);
  void enterAlternateScreen();
  void exitAlternateScreen();
  void handleResume();
  void handleResize();
  void reenterAltScreen();
  void resetFramesForAltScreen();
  void suspendStdin();
  void resumeStdin();
  void drainStdin();
  void detachForShutdown();
  void writeRaw(const std::string& data);
  void resetLineCount();
  void resetPools();
  void setCurrentNode(ReactNodeRef node){currentNode_=std::move(node);}
  void markUnmounted(bool value){
    isUnmounted_=value;
    ownerUnmounted_->store(value,std::memory_order_relaxed);
  }
  void bindInteraction(InteractionServices services);
  void bindShutdown(ShutdownServices services);
  void bindRenderCycle(RenderCycleServices services);
  void bindOwnerRoot(OwnerRootServices services);
  void bindIndigenousRenderScheduler(lodash_throttle::Services services={});
  void bindProcessLifecycle(ProcessLifecycleServices services);
  void initializeCore();
  void retainRuntime(std::shared_ptr<void> lifetime){
    runtimeLifetime_=std::move(lifetime);}
  void bindReactTree(ReactTreeServices services);
  void render(ReactNodeRef node);
  void scheduleRender(){
    if(!interaction_||!interaction_->scheduleRender)
      throw std::logic_error("Render scheduler import required");
    interaction_->scheduleRender();
  }
  std::function<void()> patchStderr(std::shared_ptr<StderrPort> stderr);
  std::function<void()> patchConsole(std::shared_ptr<ConsolePort> console,
    ConsoleServices services);
  void onRender();
  void setLastYogaCounters(YogaFrameCounters counters){
    lastYogaCounters_=counters;}
  void setLastPoolResetTime(double value){lastPoolResetTime_=value;}
  void unmount(std::exception_ptr error={});
  void setDrainTimer(event_loop::Loop::TimerHandle timer){
    drainTimer_=std::move(timer);}
  std::string copySelectionNoClear();
  std::string copySelection();
  void clearTextSelection();
  void captureScrolledRows(double firstRow,double lastRow,bool above);
  void shiftSelectionForScroll(double dRow,double minRow,double maxRow);
  void moveSelectionFocus(terminal_selection::FocusMove move);
  bool hasTextSelection()const;
  std::function<void()> subscribeToSelectionChange(SelectionListenerRef listener);
  bool dispatchClick(double col,double row);
  void dispatchHover(double col,double row);
  void dispatchKeyboardEvent(const terminal_events::ParsedKey& parsedKey);
  std::optional<std::string> getHyperlinkAt(double col,double row)const;
  void setHyperlinkClick(std::function<void(const std::string&)> callback);
  void openHyperlink(const std::string& url);
  void handleMultiClick(double col,double row,int count);
  void handleSelectionDrag(double col,double row);
  void setCursorDeclaration(CursorDeclarationRef declaration,
    std::optional<terminal_dom::ElementRef> clearIfNode=std::nullopt);
  event_loop::Deferred<event_loop::Unit> waitUntilExit();
  void resolveExitPromise();
  void rejectExitPromise(std::exception_ptr error);
  void setSearchHighlight(const std::string& query);
  std::vector<terminal_render::MatchPosition> scanElementSubtree(
    terminal_dom::DOMElement& element);
  void setSearchPositions(SearchPositionsRef state);
  void setSelectionBgColor(const std::string& color);
  const std::string& searchHighlightQuery()const{return searchHighlightQuery_;}
  const SearchPositionsRef& searchPositions()const{return searchPositions_;}
  const CursorDeclarationRef& cursorDeclaration()const{return cursorDeclaration_;}
  terminal_selection::SelectionState& selection(){return selection_;}
  const terminal_selection::SelectionState& selection()const{return selection_;}

  const terminal_renderer::Frame& frontFrame()const{return frontFrame_;}
  const terminal_renderer::Frame& backFrame()const{return backFrame_;}
  std::shared_ptr<terminal_screen::StylePool> stylePool()const{
    return stylePool_;}
  double terminalColumns()const{return terminalColumns_;}
  double terminalRows()const{return terminalRows_;}
  const std::string& altScreenParkPatch()const{return altScreenParkPatch_;}
  bool isPaused()const{return isPaused_;}
  bool isUnmounted()const{return isUnmounted_;}
  bool prevFrameContaminated()const{return prevFrameContaminated_;}
  bool needsEraseBeforePaint()const{return needsEraseBeforePaint_;}
  std::optional<CursorPoint> displayCursor()const{return displayCursor_;}
  void setDisplayCursor(std::optional<CursorPoint> cursor){
    displayCursor_=std::move(cursor);}
  bool wasRawMode()const{return wasRawMode_;}
  std::size_t storedStdinListeners()const{return stdinListeners_.size();}
private:
  OutputPort& stdout_;
  InputPort& stdin_;
  std::shared_ptr<terminal_screen::StylePool> stylePool_;
  std::shared_ptr<terminal_screen::CharPool> charPool_;
  std::shared_ptr<terminal_screen::HyperlinkPool> hyperlinkPool_;
  ControlServices services_;
  terminal_renderer::Frame frontFrame_,backFrame_;
  double terminalColumns_=80,terminalRows_=24;
  std::string altScreenParkPatch_;
  ReactNodeRef currentNode_;
  bool isUnmounted_=false,isPaused_=false;
  std::shared_ptr<std::atomic_bool> ownerUnmounted_=
    std::make_shared<std::atomic_bool>(false);
  bool altScreenActive_=false,altScreenMouseTracking_=false;
  bool prevFrameContaminated_=false,needsEraseBeforePaint_=false;
  std::optional<CursorPoint> displayCursor_;
  std::vector<StoredListener> stdinListeners_;
  bool wasRawMode_=false;
  terminal_selection::SelectionState selection_=
    terminal_selection::createSelectionState();
  std::optional<InteractionServices> interaction_;
  std::optional<ShutdownServices> shutdown_;
  std::optional<RenderCycleServices> renderCycle_;
  lodash_throttle::ThrottleRef renderThrottle_;
  ListenerRef handleResizeListener_,handleResumeListener_;
  std::function<void()> unsubscribeExit_,unsubscribeTTYHandlers_;
  std::shared_ptr<ReactTreeState> reactTree_;
  std::shared_ptr<void> runtimeLifetime_;
  event_loop::Loop::TimerHandle drainTimer_;
  double lastPoolResetTime_=0;
  YogaFrameCounters lastYogaCounters_;
  struct ListenerEntry {SelectionListenerRef listener;bool active=true;};
  std::vector<std::shared_ptr<ListenerEntry>> selectionListeners_;
  terminal_hit_test::Hovered hoveredNodes_;
  std::function<void(const std::string&)> onHyperlinkClick_;
  CursorDeclarationRef cursorDeclaration_;
  std::string searchHighlightQuery_;
  SearchPositionsRef searchPositions_;
  std::optional<event_loop::Deferred<event_loop::Unit>> exitPromise_;
  void invokeRender();
  void installReactShutdown();
  void notifySelectionChange();
};
void drainStdin(InputPort& stdin,const DrainServices& services);
} // namespace batbox::terminal_runtime

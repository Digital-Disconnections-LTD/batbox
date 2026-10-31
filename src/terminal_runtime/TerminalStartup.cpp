#include "terminal_runtime/TerminalStartup.hpp"
#include "terminal_dom/Focus.hpp"
#include "terminal_render/RenderState.hpp"
#include <stdexcept>

namespace batbox::terminal_runtime {
namespace {
struct StartupState {
  StartupOptions options;
  terminal_dom::DomServicesRef dom;
  std::shared_ptr<terminal_dom::NodeStash> stash;
  terminal_dom::ElementRef root;
  std::shared_ptr<terminal_dom::FocusManager> focus;
  std::shared_ptr<terminal_render::RenderState> renderState;
  std::shared_ptr<terminal_render::RenderNodeRenderer> renderNode;
  terminal_renderer::RendererRef renderer;
  std::shared_ptr<terminal_log_update::LogUpdate> log;
  std::shared_ptr<terminal_reconciler::ProfileCounters> profile;
  std::shared_ptr<terminal_reconciler::DebugRepaints> debugRepaints;
  std::shared_ptr<terminal_hit_test::PointerHandlerStore> pointerHandlers;
  std::shared_ptr<terminal_render::ScanState> scanState;
  std::shared_ptr<terminal_events::Dispatcher> dispatcher;
  std::shared_ptr<const terminal_color::ChalkState> chalk;
  terminal_output::OutputRenderServices outputRender;
};
void validate(const StartupOptions& options,const StartupServices& services){
  if(!options.stdout||!options.stdin||!options.stderr||
     !services.performanceNow||!services.logForDebugging||
     !services.screenDebug||!services.supportsExtendedKeys||
     !services.flushInteractionTime||!services.processLifecycle.onExit||
     !services.dom||!services.dom->createLayoutNode||
     !services.dispatcher||!services.monotonicNow||
     !services.renderNodeImports.widestLine||
     !services.outputRender.widestLine||
     !services.outputRender.sliceAnsi||!services.chalk||
     !services.setClipboard||!services.unhandledClipboardRejection||
     !services.debugRepaintsEnv||!services.oscModule||
     !services.output.getClearTerminalSequence||!services.output.link||
     !services.writeSyncStdout||!services.supportsTabStatus||
     !services.wrapForMultiplexer||!services.deleteInstance||
     !services.react.createContainer||
     !services.react.updateContainerSync||
     !services.react.flushSyncWork||
     !services.react.flushSyncFromReconciler)
    throw std::invalid_argument("Terminal startup imports required");
  if(options.patchConsole&&(!services.console||!services.consoleServices.format||
      !services.consoleServices.truthy||!services.consoleServices.logError||
      !std::dynamic_pointer_cast<StderrPort>(options.stderr)))
    throw std::invalid_argument("Console and stderr patch imports required");
}
}
std::shared_ptr<TerminalControls> createTerminalControls(
    StartupOptions options,StartupServices services){
  validate(options,services);
  auto state=std::make_shared<StartupState>();
  state->options=options;
  state->dom=services.dom;
  state->dispatcher=services.dispatcher;
  state->chalk=services.chalk;
  state->outputRender=services.outputRender;

  ControlServices control;
  control.supportsExtendedKeys=services.supportsExtendedKeys;
  control.logForDebugging=services.logForDebugging;
  control.screenDebug=services.screenDebug;
  control.ownerLoop=&services.ownerLoop;
  auto owner=std::make_shared<TerminalControls>(
    *options.stdout,*options.stdin,std::move(control));
  owner->retainRuntime(state);
  owner->setLastPoolResetTime(services.performanceNow());

  std::function<void()> restoreConsole,restoreStderr;
  if(options.patchConsole){
    restoreConsole=owner->patchConsole(services.console,
      services.consoleServices);
    restoreStderr=owner->patchStderr(
      std::dynamic_pointer_cast<StderrPort>(options.stderr));
  }
  owner->initializeCore();
  state->log=std::make_shared<terminal_log_update::LogUpdate>(
    terminal_log_update::Options{options.stdout->isTTY(),
      owner->stylePool()},
    terminal_log_update::Services{services.performanceNow,
      [log=services.logForDebugging](const std::string& text){
        log(text,"");},services.oscModule});

  owner->bindIndigenousRenderScheduler();
  owner->bindProcessLifecycle(std::move(services.processLifecycle));

  state->root=terminal_dom::createNode("ink-root",state->dom);
  state->stash=std::make_shared<terminal_dom::NodeStash>();
  state->profile=std::make_shared<terminal_reconciler::ProfileCounters>(
    services.performanceNow);
  state->debugRepaints=
    std::make_shared<terminal_reconciler::DebugRepaints>(
      services.debugRepaintsEnv);
  state->pointerHandlers=
    std::make_shared<terminal_hit_test::PointerHandlerStore>();
  state->scanState=std::make_shared<terminal_render::ScanState>();
  state->scanState->performanceNow=services.performanceNow;
  state->focus=std::make_shared<terminal_dom::FocusManager>(
    [dispatcher=state->dispatcher](terminal_dom::DOMElement& target,
        terminal_events::FocusEvent& event){
      return dispatcher->dispatchDiscrete(target,event);},
    services.monotonicNow);

  state->renderState=
    std::make_shared<terminal_render::RenderState>(*state->stash);
  state->renderNode=std::make_shared<terminal_render::RenderNodeRenderer>(
    *state->renderState,std::move(services.renderNodeImports));
  terminal_renderer::RendererServices renderer;
  renderer.resetLayoutShifted=[renderState=state->renderState]{
    renderState->resetLayoutShifted();};
  renderer.resetScrollHint=[renderState=state->renderState]{
    renderState->resetScrollHint();};
  renderer.resetScrollDrainNode=[renderState=state->renderState]{
    renderState->resetScrollDrainNode();};
  renderer.renderNodeToOutput=[renderNode=state->renderNode](
      terminal_dom::DOMElement& node,
      terminal_output::Output& output,
      terminal_screen::ScreenRef previous){
    terminal_render::RenderNodeOptions options;
    options.prevScreen=std::move(previous);
    renderNode->render(node,output,std::move(options));
  };
  renderer.getScrollDrainNode=[renderState=state->renderState]{
    return renderState->getScrollDrainNode();};
  renderer.getScrollHint=[renderState=state->renderState]{
    const auto hint=renderState->getScrollHint();
    if(!hint)return std::optional<terminal_renderer::ScrollHint>{};
    return std::optional<terminal_renderer::ScrollHint>{
      terminal_renderer::ScrollHint{hint->top,hint->bottom,hint->delta}};
  };
  renderer.outputRender=state->outputRender;
  renderer.logForDebugging=services.logForDebugging;
  state->renderer=terminal_renderer::createRenderer(*state->root,
    owner->stylePool(),*state->stash,std::move(renderer));

  owner->bindOwnerRoot(OwnerRootServices{state->root,state->focus.get(),
    state->profile.get(),services.performanceNow,
    [weak=std::weak_ptr<TerminalControls>(owner)]{
      if(auto current=weak.lock())current->scheduleRender();}});
  InteractionServices interaction;
  interaction.rootNode=state->root;
  interaction.focusManager=state->focus.get();
  interaction.nodeStash=state->stash.get();
  interaction.pointerHandlers=state->pointerHandlers.get();
  interaction.dispatcher=state->dispatcher.get();
  interaction.monotonicNow=services.monotonicNow;
  interaction.renderNode=state->renderNode.get();
  interaction.outputRender=&state->outputRender;
  interaction.scanState=state->scanState.get();
  interaction.chalk=state->chalk.get();
  interaction.setClipboard=std::move(services.setClipboard);
  interaction.unhandledClipboardRejection=
    std::move(services.unhandledClipboardRejection);
  owner->bindInteraction(std::move(interaction));

  RenderCycleServices cycle;
  cycle.flushInteractionTime=services.flushInteractionTime;
  cycle.performanceNow=services.performanceNow;
  cycle.renderer=state->renderer.get();
  cycle.renderState=state->renderState.get();
  cycle.log=state->log.get();
  cycle.profile=state->profile.get();
  cycle.debugRepaints=state->debugRepaints.get();
  cycle.output=services.output;
  cycle.syncOutputSupported=services.syncOutputSupported;
  cycle.onFrame=options.onFrame;
  owner->bindRenderCycle(std::move(cycle));

  ShutdownServices shutdown;
  shutdown.log=state->log.get();
  shutdown.output=std::move(services.output);
  shutdown.sequences=std::move(services.sequences);
  shutdown.writeSyncStdout=std::move(services.writeSyncStdout);
  shutdown.supportsTabStatus=std::move(services.supportsTabStatus);
  shutdown.wrapForMultiplexer=std::move(services.wrapForMultiplexer);
  shutdown.restoreConsole=std::move(restoreConsole);
  shutdown.restoreStderr=std::move(restoreStderr);
  shutdown.deleteInstance=std::move(services.deleteInstance);
  shutdown.rootNode=state->root;
  owner->bindShutdown(std::move(shutdown));

  services.react.stdin=options.stdin;
  services.react.stdout=options.stdout;
  services.react.stderr=options.stderr;
  services.react.exitOnCtrlC=options.exitOnCtrlC;
  owner->bindReactTree(std::move(services.react));
  return owner;
}
} // namespace batbox::terminal_runtime

#include "terminal_runtime/TerminalControls.hpp"
#include "terminal_dom/FullLayoutPort.hpp"
#include "terminal_optimizer/Optimizer.hpp"
#include "terminal_log_update/PatchJson.hpp"
#include <stdexcept>

namespace batbox::terminal_runtime {
void TerminalControls::bindShutdown(ShutdownServices services){
  shutdown_=std::move(services);
  installReactShutdown();
}
void TerminalControls::unmount(std::exception_ptr error){
  if(isUnmounted_)return;
  if(!shutdown_||!shutdown_->log||!shutdown_->writeSyncStdout||
     !shutdown_->supportsTabStatus||!shutdown_->wrapForMultiplexer||
     !shutdown_->updateContainerNull||!shutdown_->flushSyncWork||
     !shutdown_->deleteInstance||!shutdown_->rootNode)
    throw std::logic_error("Shutdown imports required");
  invokeRender();
  if(unsubscribeExit_)unsubscribeExit_();
  else if(shutdown_->unsubscribeExit)shutdown_->unsubscribeExit();
  if(shutdown_->restoreConsole){
    shutdown_->restoreConsole();
    shutdown_->restoreConsole={};
  }
  if(shutdown_->restoreStderr){
    shutdown_->restoreStderr();
    shutdown_->restoreStderr={};
  }
  if(unsubscribeTTYHandlers_)unsubscribeTTYHandlers_();
  else if(shutdown_->unsubscribeTTYHandlers)
    shutdown_->unsubscribeTTYHandlers();

  const auto diff=std::make_shared<terminal_renderer::Diff>(
    shutdown_->log->renderPreviousOutput_DEPRECATED(frontFrame_));
  const auto optimized=terminal_optimizer::optimize(diff);
  terminal_log_update::writeDiffToTerminal(
    [this](const std::string& text){stdout_.write(text);},
    *optimized,false,shutdown_->output);

  if(stdout_.isTTY()){
    const auto& seq=shutdown_->sequences;
    auto& write=shutdown_->writeSyncStdout;
    if(altScreenActive_)write(seq.exitAltScreen);
    write(seq.disableMouseTracking);
    drainStdin();
    write(seq.disableModifyOtherKeys);
    write(seq.disableKittyKeyboard);
    write(seq.disableFocusEvents);
    write(seq.disableBracketedPaste);
    write(seq.showCursor);
    write(seq.clearProgress);
    if(shutdown_->supportsTabStatus())
      write(shutdown_->wrapForMultiplexer(seq.clearTabStatus));
  }

  isUnmounted_=true;
  ownerUnmounted_->store(true,std::memory_order_relaxed);
  if(services_.cancelScheduledRender)services_.cancelScheduledRender();
  if(drainTimer_.valid()){
    drainTimer_.cancel();
    drainTimer_={};
  }
  shutdown_->updateContainerNull();
  shutdown_->flushSyncWork();
  shutdown_->updateContainerNull={};
  shutdown_->flushSyncWork={};
  reactTree_.reset();
  shutdown_->deleteInstance();
  if(shutdown_->rootNode->yogaNode){
    const auto full=std::dynamic_pointer_cast<terminal_dom::FullLayoutNode>(
      shutdown_->rootNode->yogaNode);
    if(!full)throw std::logic_error("Full layout node required");
    full->free();
    shutdown_->rootNode->yogaNode.reset();
  }
  if(error)rejectExitPromise(error);
  else resolveExitPromise();
}
} // namespace batbox::terminal_runtime

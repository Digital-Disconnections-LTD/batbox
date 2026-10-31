#include "terminal_runtime/TerminalControls.hpp"
#include <stdexcept>

namespace batbox::terminal_runtime {
event_loop::Deferred<event_loop::Unit> TerminalControls::waitUntilExit(){
  if(!exitPromise_){
    if(!services_.ownerLoop)
      throw std::logic_error("Exit Promise owner loop required");
    exitPromise_.emplace(*services_.ownerLoop);
  }
  return event_loop::adoptAsyncReturn(*exitPromise_);
}
void TerminalControls::resolveExitPromise(){
  if(exitPromise_)exitPromise_->resolve({});
}
void TerminalControls::rejectExitPromise(std::exception_ptr error){
  if(exitPromise_)exitPromise_->reject(std::move(error));
}
} // namespace batbox::terminal_runtime

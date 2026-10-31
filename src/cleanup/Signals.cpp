#include "cleanup/Signals.hpp"
#include <csignal>
#include <stdexcept>

namespace batbox::cleanup {
struct SignalLifecycle::State {
  SignalServices services;
  bool installed=false;
  std::optional<ShutdownTimer> orphanTimer;
  explicit State(SignalServices imports):services(std::move(imports)){}
};
namespace {
void require(const SignalServices& services){
  if(!services.pinExitHook||!services.onSignal||!services.onUncaughtException||
    !services.onUnhandledRejection||!services.setInterval||!services.isPrintMode||
    !services.isWindows||!services.stdinIsTTY||!services.isScrollDraining||
    !services.stdoutWritable||!services.stdinReadable||!services.localDiagnostic||
    !services.gracefulShutdown)
    throw std::invalid_argument("signal shutdown imports required");
}
}
SignalLifecycle::SignalLifecycle(SignalServices services):
  state_(std::make_shared<State>(std::move(services))){require(state_->services);}
bool SignalLifecycle::installed()const{return state_->installed;}
void SignalLifecycle::setupGracefulShutdown(){
  auto state=state_;
  if(state->installed)return;
  auto& services=state->services;
  services.pinExitHook();
  services.onSignal(SIGINT,[state]{
    if(state->services.isPrintMode())return;
    state->services.localDiagnostic("info","shutdown_signal",Json{{"signal","SIGINT"}});
    state->services.gracefulShutdown(0);
  });
  services.onSignal(SIGTERM,[state]{
    state->services.localDiagnostic("info","shutdown_signal",Json{{"signal","SIGTERM"}});
    state->services.gracefulShutdown(143);
  });
  if(!services.isWindows()){
#ifndef _WIN32
    services.onSignal(SIGHUP,[state]{
      state->services.localDiagnostic("info","shutdown_signal",Json{{"signal","SIGHUP"}});
      state->services.gracefulShutdown(129);
    });
    if(services.stdinIsTTY()){
      std::weak_ptr<State> weak=state;
      state->orphanTimer=services.setInterval(30000,[weak]{
        const auto state=weak.lock();if(!state)return;
        auto& imports=state->services;
        if(imports.isScrollDraining())return;
        if(!imports.stdoutWritable()||!imports.stdinReadable()){
          if(state->orphanTimer){state->orphanTimer->cancel();state->orphanTimer.reset();}
          imports.localDiagnostic("info","shutdown_signal",Json{{"signal","orphan_detected"}});
          imports.gracefulShutdown(129);
        }
      });
      state->orphanTimer->unref();
    }
#endif
  }
  services.onUncaughtException([state](DiagnosticError error){
    state->services.localDiagnostic("error","uncaught_exception",
      Json{{"error_name",error.name},{"error_message",value::slice(error.message,0,2000)}});
  });
  services.onUnhandledRejection([state](RejectionReason reason){
    Json info;
    if(reason.error){
      info={{"error_name",reason.error->name},
        {"error_message",value::slice(reason.error->message,0,2000)}};
      if(reason.error->stack)info["error_stack"]=value::slice(*reason.error->stack,0,4000);
    }else info={{"error_message",value::slice(reason.stringValue,0,2000)}};
    state->services.localDiagnostic("error","unhandled_rejection",std::move(info));
  });
  state->installed=true;
}
} // namespace batbox::cleanup

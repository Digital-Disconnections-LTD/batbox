#pragma once
#include "interactive/lifecycle/LoadingState.hpp"

namespace batbox::interactive::navigation {
struct DialogInputs {
  bool isExiting=false,hasExitFlow=false,messageSelectorVisible=false;
  bool poetryInputActive=false,sandboxPermission=false,gadgetPermission=false;
  bool poetryRequest=false,workerSandboxPermission=false,elicitation=false;
  bool isLoading=false,showCostDialog=false,idleReturnPending=false;
  bool ultraplanEnabled=false,ultraplanPendingChoice=false,
    ultraplanLaunchPending=false;
  bool showIdeOnboarding=false,showEffortCallout=false,
    showRemoteCallout=false,lspRecommendation=false,hintRecommendation=false,
    showDesktopUpsellStartup=false;
  bool hasGadgetUi=false,gadgetUiShouldContinueAnimation=false;
};
using Dialog=std::optional<std::string>;
Dialog focusedInputDialog(const DialogInputs& input);
bool hasSuppressedDialogs(const DialogInputs& input);

class DialogFocus {
public:
  DialogFocus(lifecycle::LoadingState& loading,
    std::function<void()> repinScroll):loading_(loading),
    repinScroll_(std::move(repinScroll)){}
  Dialog observe(const DialogInputs& input,double nowMs);
  const Dialog& current()const{return current_;}
private:
  lifecycle::LoadingState& loading_;
  std::function<void()> repinScroll_;
  Dialog current_;
  bool initialized_=false;
};
} // namespace batbox::interactive::navigation

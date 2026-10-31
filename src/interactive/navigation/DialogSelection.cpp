#include "interactive/navigation/DialogFocus.hpp"

namespace batbox::interactive::navigation {
Dialog focusedInputDialog(const DialogInputs& in){
  if(in.isExiting||in.hasExitFlow)return {};
  if(in.messageSelectorVisible)return "message-selector";
  if(in.poetryInputActive)return {};
  if(in.sandboxPermission)return "sandbox-permission";
  const bool allowed=!in.hasGadgetUi||in.gadgetUiShouldContinueAnimation;
  if(allowed&&in.gadgetPermission)return "tool-permission";
  if(allowed&&in.poetryRequest)return "prompt";
  if(allowed&&in.workerSandboxPermission)return "worker-sandbox-permission";
  if(allowed&&in.elicitation)return "elicitation";
  if(allowed&&!in.isLoading&&in.showCostDialog)return "cost";
  if(allowed&&in.idleReturnPending)return "idle-return";
  if(in.ultraplanEnabled&&allowed&&!in.isLoading&&in.ultraplanPendingChoice)
    return "ultraplan-choice";
  if(in.ultraplanEnabled&&allowed&&!in.isLoading&&in.ultraplanLaunchPending)
    return "ultraplan-launch";
  if(allowed&&in.showIdeOnboarding)return "ide-onboarding";
  if(allowed&&in.showEffortCallout)return "effort-callout";
  if(allowed&&in.showRemoteCallout)return "remote-callout";
  if(allowed&&in.lspRecommendation)return "lsp-recommendation";
  if(allowed&&in.hintRecommendation)return "plugin-hint";
  if(allowed&&in.showDesktopUpsellStartup)return "desktop-upsell";
  return {};
}
bool hasSuppressedDialogs(const DialogInputs& in){
  return in.poetryInputActive&&(in.sandboxPermission||in.gadgetPermission||
    in.poetryRequest||in.workerSandboxPermission||in.elicitation||
    (!in.isLoading&&in.showCostDialog));
}
} // namespace batbox::interactive::navigation

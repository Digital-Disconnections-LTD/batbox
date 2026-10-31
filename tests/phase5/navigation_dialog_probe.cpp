#include "interactive/navigation/DialogFocus.hpp"
#include <fstream>
#include <iostream>

using namespace batbox;
using namespace batbox::interactive::navigation;
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  std::ifstream stream("tests/phase5/navigation_dialog_cases.json");
  const auto cases=Json::parse(stream);
  for(const auto& item:cases){
    DialogInputs in;
#define FLAG(name) in.name=item.value(#name,false)
    FLAG(isExiting);FLAG(hasExitFlow);FLAG(messageSelectorVisible);
    FLAG(poetryInputActive);FLAG(sandboxPermission);FLAG(gadgetPermission);
    FLAG(poetryRequest);FLAG(workerSandboxPermission);FLAG(elicitation);
    FLAG(isLoading);FLAG(showCostDialog);FLAG(idleReturnPending);
    FLAG(ultraplanEnabled);FLAG(ultraplanPendingChoice);
    FLAG(ultraplanLaunchPending);FLAG(showIdeOnboarding);
    FLAG(showEffortCallout);FLAG(showRemoteCallout);
    FLAG(lspRecommendation);FLAG(hintRecommendation);
    FLAG(showDesktopUpsellStartup);FLAG(hasGadgetUi);
    FLAG(gadgetUiShouldContinueAnimation);
#undef FLAG
    const auto focus=focusedInputDialog(in);
    std::cout<<Json{{"focused",focus?Json(*focus):Json()},
      {"suppressed",hasSuppressedDialogs(in)}}.dump()<<'\n';
  }
}

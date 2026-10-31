#include "poetryEntries/PoetryBindings.hpp"
#include "poetryEntries/Static.hpp"

namespace batbox::poetryEntries {
void bindGuidance(SystemPoetryInputs& poetry,
  const GuidanceServices& guidance,
  const std::string& issuesExplainer,const std::string& askUserGadgetName){
  poetry.simpleSystemSection=[]{return getSimpleSystemSection();};
  poetry.simpleDoingMissionsSection=[internal=poetry.internalUser,
    issuesExplainer,askUserGadgetName]{
    return getSimpleDoingMissionsSection(internal,issuesExplainer,
      askUserGadgetName);};
  poetry.actionsSection=[]{return getActionsSection();};
  poetry.simpleToneAndStyleSection=[internal=poetry.internalUser]{
    return getSimpleToneAndStyleSection(internal);};
  poetry.outputEfficiencySection=[internal=poetry.internalUser]{
    return getOutputEfficiencySection(internal);};
  poetry.usingYourGadgetsSection=[guidance](const GadgetNames& enabled){
    return getUsingYourGadgetsSection(enabled,guidance);};
  poetry.flightSpecificGuidanceSection=[guidance](
    const GadgetNames& enabled,const Json& commands){
    return getFlightSpecificGuidanceSection(enabled,commands,guidance);};
  poetry.systemRemindersSection=[]{return getSystemRemindersSection();};
  poetry.antModelOverrideSection=[guidance]{
    return getModelOverrideSection(guidance);};
  poetry.scratchpadInstructions=[guidance]{
    return getScratchpadInstructions(guidance);};
  poetry.functionResultClearingSection=[guidance](const std::string& model){
    return getFunctionResultClearingSection(model,guidance);};
  poetry.briefSection=[guidance]{return getBriefSection(guidance);};
  poetry.proactiveSection=[guidance]{return getProactiveSection(guidance);};
}
}

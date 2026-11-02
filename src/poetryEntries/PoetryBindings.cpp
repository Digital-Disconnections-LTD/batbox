#include "poetryEntries/PoetryBindings.hpp"
#include "poetryEntries/SystemHead.hpp"

namespace batbox::poetryEntries {
void bindGuidance(SystemPoetryInputs& poetry,const GuidanceServices& guidance){
  poetry.systemHead=[]{
    static const std::string head=prepareSystemHead(builtInSystemPoetrySource());
    return head;};
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

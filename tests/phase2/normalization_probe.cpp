#include "normalization/Normalization.hpp"
#include <iostream>
#include <string>

using batbox::Json;
namespace norm=batbox::normalization;

static Json evaluate(const Json& input){
  int uuidCounter=0;
  batbox::messages::Services messageServices;
  messageServices.randomUUID=[&uuidCounter](){return "fixture-uuid-"+std::to_string(++uuidCounter);};
  messageServices.nowISO=[](){return "2026-10-28T00:00:00.000Z";};
  messageServices.strictGadgetResultPairing=[](){return false;};
  messageServices.featureGate=[&input](const std::string& gate){
    const Json& gates=batbox::value::property(input,"gates");
    return gates.is_object() && gates.value(gate,false);
  };
  messageServices.logError=[](const std::string&){};
  norm::Dependencies deps;
  deps.messageServices=&messageServices;
  deps.featureGate=messageServices.featureGate;
  deps.gadgetSearchEnabledOptimistic=[&input](){return input.value("toolSearchOptimistic",false);};
  deps.gadgetMatchesName=[](const Json& gadget,const std::string& name){
    if(gadget.value("name","")==name) return true;
    const Json aliases=gadget.value("aliases",Json::array());
    for(const auto& alias:aliases) if(alias==name) return true;
    return false;
  };
  deps.normalizeLegacyGadgetName=[&input](const std::string& name){
    const Json& aliases=batbox::value::property(input,"legacyAliases");
    return aliases.is_object() && aliases.contains(name)?aliases[name].get<std::string>():name;
  };
  deps.normalizeGadgetInputForAPI=[&input](const Json& gadget,const Json& value){
    if(!value.is_object()) return value;
    Json result=value;
    const Json& changes=batbox::value::property(input,"normalizeInputRemove");
    const std::string name=gadget.value("name","");
    if(changes.is_object() && changes.contains(name))
      for(const auto& key:changes[name]) result.erase(key.get<std::string>());
    return result;
  };
  deps.normalizeAttachmentForAPI=[&input](const Json& attachment){
    const Json& outputs=batbox::value::property(input,"attachmentOutputs");
    const std::string type=attachment.value("type","");
    return outputs.is_object() && outputs.contains(type)?outputs[type]:Json::array();
  };
  deps.validateImagesForAPI=norm::validateImagesForAPI;
  deps.mediaErrorMessages=input.value("mediaErrorMessages",Json::object());
  deps.historySnipFeature=input.value("historySnipFeature",false);
  deps.historySnipRuntimeEnabled=input.value("historySnipRuntimeEnabled",false);
  deps.testEnvironment=input.value("testEnvironment",true);
  const std::string op=input.at("op").get<std::string>();
  if(op=="normalize")return norm::normalizeMessagesForAPI(input.at("messages"),input.value("tools",Json::array()),deps);
  if(op=="prepare")return norm::prepareMessagesForRequest(input.at("messages"),input.value("tools",Json::array()),
    input.value("modelToolSearchEnabled",false),input.value("advisorBetaEnabled",false),deps);
  if(op=="stripReferences")return norm::stripGadgetReferenceBlocksFromUserMessage(input.at("message"));
  if(op=="stripCaller")return norm::stripCallerFieldFromAssistantMessage(input.at("message"));
  if(op=="reorder")return norm::reorderAttachmentsForAPI(input.at("messages"));
  if(op=="mergeUser")return norm::mergeUserMessages(input.at("a"),input.at("b"),deps);
  if(op=="mergeAssistant")return norm::mergeAssistantMessages(input.at("a"),input.at("b"));
  if(op=="orphanThinking")return norm::filterOrphanedThinkingOnlyMessages(input.at("messages"));
  if(op=="trailingThinking")return norm::filterTrailingThinkingFromLastAssistant(input.at("messages"));
  if(op=="whitespace")return norm::filterWhitespaceOnlyAssistantMessages(input.at("messages"),deps);
  if(op=="nonempty")return norm::ensureNonEmptyAssistantContent(input.at("messages"),deps);
  if(op=="sanitizeError")return norm::sanitizeErrorGadgetResultContent(input.at("messages"));
  if(op=="advisor")return norm::stripAdvisorBlocks(input.at("messages"));
  if(op=="imageValidation"){norm::validateImagesForAPI(input.at("messages"));return true;}
  if(op=="tag")return norm::appendMessageTagToUserMessage(input.at("message"));
  throw std::runtime_error("Unknown operation");
}
int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<Json{{"result",evaluate(Json::parse(line))}}.dump()<<'\n';}
  catch(const std::exception& error){std::cout<<Json{{"error",error.what()}}.dump()<<'\n';}
}}

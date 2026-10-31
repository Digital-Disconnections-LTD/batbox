#include "normalization/Internal.hpp"
#include "requests/assembly.hpp"
#include <map>
#include <set>

namespace batbox::normalization {
using namespace internal;
namespace {
bool syntheticApiError(const Json& message,const Dependencies& deps) {
  return typeIs(message,"assistant") && get(message,"isApiErrorMessage")==true &&
    get(get(message,"message"),"model")==deps.syntheticModel;
}
bool gadgetResultMessage(const Json& message) {
  if(!typeIs(message,"user")) return false;
  const Json& content=get(get(message,"message"),"content");
  if(!content.is_array()) return false;
  for(const auto& block:content) if(typeIs(block,"tool_result")) return true;
  return false;
}
Json ensureSystemReminderWrap(const Json& message) {
  const Json& content=get(get(message,"message"),"content");
  if(content.is_string()) {
    const std::string value=content.get<std::string>();
    if(value.rfind("<system-reminder>",0)==0) return message;
    Json copy=message;copy["message"]["content"]="<system-reminder>\n"+value+"\n</system-reminder>";return copy;
  }
  bool changed=false;Json mapped=Json::array();
  for(const auto& block:content) {
    if(typeIs(block,"text") && get(block,"text").is_string() &&
       get(block,"text").get<std::string>().rfind("<system-reminder>",0)!=0) {
      Json copy=block;
      copy["text"]="<system-reminder>\n"+get(block,"text").get<std::string>()+"\n</system-reminder>";
      mapped.push_back(std::move(copy));changed=true;
    } else mapped.push_back(block);
  }
  if(!changed) return message;
  Json copy=message;copy["message"]["content"]=std::move(mapped);return copy;
}
} // namespace

Json normalizeMessagesForAPI(const Json& messages,const Json& gadgets,const Dependencies& deps) {
  std::set<std::string> availableGadgetNames;
  for(const auto& gadget:gadgets) availableGadgetNames.insert(get(gadget,"name").get<std::string>());
  Json reordered=Json::array();
  for(const auto& message:reorderAttachmentsForAPI(messages))
    if(!((typeIs(message,"user") || typeIs(message,"assistant")) && value::truthy(get(message,"isVirtual"))))
      reordered.push_back(message);

  std::map<std::string,std::set<std::string>> stripTargets;
  for(std::size_t i=0;i<reordered.size();++i) {
    const Json& message=reordered[i];
    if(!syntheticApiError(message,deps)) continue;
    const Json& content=get(get(message,"message"),"content");
    if(!content.is_array() || content.empty() || !typeIs(content[0],"text")) continue;
    const Json& errorText=get(content[0],"text");
    if(!value::truthy(errorText)) continue;
    const std::string error=errorText.get<std::string>();
    if(!deps.mediaErrorMessages.is_object() || !deps.mediaErrorMessages.contains(error)) continue;
    for(int j=static_cast<int>(i)-1;j>=0;--j) {
      const Json& candidate=reordered[static_cast<std::size_t>(j)];
      if(typeIs(candidate,"user") && value::truthy(get(candidate,"isMeta"))) {
        const std::string uuid=get(candidate,"uuid").get<std::string>();
        for(const auto& type:deps.mediaErrorMessages[error]) stripTargets[uuid].insert(type.get<std::string>());
        break;
      }
      if(syntheticApiError(candidate,deps)) continue;
      break;
    }
  }

  Json result=Json::array();
  for(const auto& message:reordered) {
    if(typeIs(message,"progress") || (typeIs(message,"system") && get(message,"subtype")!="local_command") ||
       syntheticApiError(message,deps)) continue;
    if(typeIs(message,"system")) {
      need(deps.messageServices!=nullptr,"messageServices");
      Json args={{"content",get(message,"content")}};
      if(has(message,"uuid")) args["uuid"]=message["uuid"];
      if(has(message,"timestamp")) args["timestamp"]=message["timestamp"];
      Json user=messages::createUserMessage(args,*deps.messageServices);
      if(!result.empty() && typeIs(result.back(),"user")) result.back()=mergeUserMessages(result.back(),user,deps);
      else result.push_back(std::move(user));
      continue;
    }
    if(typeIs(message,"user")) {
      need(static_cast<bool>(deps.gadgetSearchEnabledOptimistic),"toolSearchEnabledOptimistic");
      Json normalized=deps.gadgetSearchEnabledOptimistic()
        ? stripUnavailableGadgetReferencesFromUserMessage(message,availableGadgetNames,deps)
        : stripGadgetReferenceBlocksFromUserMessage(message);
      const Json& uuid=get(normalized,"uuid");
      const auto target=uuid.is_string()?stripTargets.find(uuid.get<std::string>()):stripTargets.end();
      if(target!=stripTargets.end() && value::truthy(get(normalized,"isMeta"))) {
        const Json& content=get(get(normalized,"message"),"content");
        if(content.is_array()) {
          Json filtered=Json::array();
          for(const auto& block:content) {
            const Json& type=get(block,"type");
            if(!type.is_string() || !target->second.contains(type.get<std::string>())) filtered.push_back(block);
          }
          if(filtered.empty()) continue;
          if(filtered.size()<content.size()) normalized["message"]["content"]=std::move(filtered);
        }
      }
      need(static_cast<bool>(deps.featureGate),"featureGate");
      if(!deps.featureGate("tengu_toolref_defer_j8m")) {
        const Json& content=get(get(normalized,"message"),"content");
        if(content.is_array()) {
          bool hasBoundary=false;
          for(const auto& block:content) if(typeIs(block,"text") && get(block,"text").is_string() &&
            get(block,"text").get<std::string>().rfind(deps.gadgetReferenceTurnBoundary,0)==0) hasBoundary=true;
          if(!hasBoundary && contentHasGadgetReference(content))
            normalized["message"]["content"].push_back(textBlock(deps.gadgetReferenceTurnBoundary));
        }
      }
      if(!result.empty() && typeIs(result.back(),"user")) result.back()=mergeUserMessages(result.back(),normalized,deps);
      else result.push_back(std::move(normalized));
      continue;
    }
    if(typeIs(message,"assistant")) {
      need(static_cast<bool>(deps.gadgetSearchEnabledOptimistic),"toolSearchEnabledOptimistic");
      const bool gadgetSearch=deps.gadgetSearchEnabledOptimistic();
      Json normalized=message,mapped=Json::array();
      for(const auto& block:get(get(message,"message"),"content")) {
        if(!typeIs(block,"tool_use")) {mapped.push_back(block);continue;}
        const Json* found=nullptr;
        for(const auto& gadget:gadgets) {
          need(static_cast<bool>(deps.gadgetMatchesName),"toolMatchesName");
          if(deps.gadgetMatchesName(gadget,get(block,"name").get<std::string>())) {found=&gadget;break;}
        }
        Json input=get(block,"input");
        std::string canonical=get(block,"name").get<std::string>();
        if(found) {
          need(static_cast<bool>(deps.normalizeGadgetInputForAPI),"normalizeToolInputForAPI");
          input=deps.normalizeGadgetInputForAPI(*found,input);
          canonical=get(*found,"name").get<std::string>();
        }
        if(gadgetSearch) {
          Json copy=block;copy["name"]=canonical;copy["input"]=input;mapped.push_back(std::move(copy));
        } else {
          Json copy={{"type","tool_use"}};
          if(has(block,"id")) copy["id"]=block["id"];
          copy["name"]=canonical;copy["input"]=input;mapped.push_back(std::move(copy));
        }
      }
      normalized["message"]["content"]=std::move(mapped);
      bool merged=false;
      for(int i=static_cast<int>(result.size())-1;i>=0;--i) {
        const Json& previous=result[static_cast<std::size_t>(i)];
        if(!typeIs(previous,"assistant") && !gadgetResultMessage(previous)) break;
        if(typeIs(previous,"assistant") && get(get(previous,"message"),"id")==get(get(normalized,"message"),"id")) {
          result[static_cast<std::size_t>(i)]=mergeAssistantMessages(previous,normalized);merged=true;break;
        }
      }
      if(!merged) result.push_back(std::move(normalized));
      continue;
    }
    if(typeIs(message,"attachment")) {
      need(static_cast<bool>(deps.normalizeAttachmentForAPI),"normalizeAttachmentForAPI");
      Json attachmentMessages=deps.normalizeAttachmentForAPI(get(message,"attachment"));
      need(static_cast<bool>(deps.featureGate),"featureGate");
      if(deps.featureGate("tengu_chair_sermon")) {
        Json wrapped=Json::array();
        for(const auto& attachment:attachmentMessages) wrapped.push_back(ensureSystemReminderWrap(attachment));
        attachmentMessages=std::move(wrapped);
      }
      if(!result.empty() && typeIs(result.back(),"user")) {
        Json merged=result.back();
        for(const auto& attachment:attachmentMessages) merged=mergeUserMessagesAndGadgetResults(merged,attachment,deps);
        result.back()=std::move(merged);
      } else for(const auto& attachment:attachmentMessages) result.push_back(attachment);
    }
  }
  need(static_cast<bool>(deps.featureGate),"featureGate");
  Json relocated=deps.featureGate("tengu_toolref_defer_j8m")?relocateGadgetReferenceSiblings(result):result;
  Json withFilteredOrphans=filterOrphanedThinkingOnlyMessages(relocated);
  Json withFilteredThinking=filterTrailingThinkingFromLastAssistant(withFilteredOrphans);
  Json withFilteredWhitespace=filterWhitespaceOnlyAssistantMessages(withFilteredThinking,deps);
  Json withNonEmpty=ensureNonEmptyAssistantContent(withFilteredWhitespace,deps);
  Json smooshed=deps.featureGate("tengu_chair_sermon")
    ? smooshSystemReminderSiblings(mergeAdjacentUserMessages(withNonEmpty,deps)) : withNonEmpty;
  Json sanitized=sanitizeErrorGadgetResultContent(smooshed);
  if(deps.historySnipFeature && !deps.testEnvironment && deps.historySnipRuntimeEnabled) {
    for(auto& item:sanitized) if(typeIs(item,"user")) item=appendMessageTagToUserMessage(item);
  }
  need(static_cast<bool>(deps.validateImagesForAPI),"validateImagesForAPI");
  deps.validateImagesForAPI(sanitized);
  return sanitized;
}

Json prepareMessagesForRequest(const Json& messages,const Json& gadgets,
                               bool modelGadgetSearchEnabled,bool advisorBetaEnabled,
                               const Dependencies& deps) {
  Json normalized=normalizeMessagesForAPI(messages,gadgets,deps);
  if(!modelGadgetSearchEnabled) {
    Json mapped=Json::array();
    for(const auto& message:normalized) {
      if(typeIs(message,"user")) mapped.push_back(stripGadgetReferenceBlocksFromUserMessage(message));
      else if(typeIs(message,"assistant")) mapped.push_back(stripCallerFieldFromAssistantMessage(message));
      else mapped.push_back(message);
    }
    normalized=std::move(mapped);
  }
  need(deps.messageServices!=nullptr,"messageServices");
  normalized=messages::ensureGadgetResultPairing(normalized,*deps.messageServices);
  if(!advisorBetaEnabled) normalized=stripAdvisorBlocks(normalized);
  return requests::stripExcessMediaItems(normalized,100);
}

} // namespace batbox::normalization

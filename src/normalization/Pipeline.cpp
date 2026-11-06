#include "normalization/Internal.hpp"
#include "requests/assembly.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <vector>

namespace batbox::normalization {
using namespace internal;
namespace {
bool syntheticApiError(const Json& message,const Dependencies& deps) {
  return typeIs(message,"assistant") && get(message,"isApiErrorMessage")==true &&
    value::stringEquals(get(get(message,"message"),"model"),deps.syntheticModel);
}
bool gadgetResultMessage(const Json& message) {
  if(!typeIs(message,"user")) return false;
  const Json& content=get(get(message,"message"),"content");
  if(!content.is_array()) return false;
  for(const auto& block:content) if(typeIs(block,"tool_result")) return true;
  return false;
}
// Messages in API order, each attachment moved up to just after the last
// assistant message or tool result before it, without the virtual messages,
// as pointers into messages.
std::vector<const Json*> reorderedForAPI(const Json& messages) {
  std::vector<const Json*> result;
  result.reserve(messages.size());
  std::vector<const Json*> pendingAttachments;
  for(int i=static_cast<int>(messages.size())-1;i>=0;--i) {
    const Json& message=messages[static_cast<std::size_t>(i)];
    if(typeIs(message,"attachment")) {pendingAttachments.push_back(&message);continue;}
    const Json& content=get(get(message,"message"),"content");
    const bool stoppingPoint=typeIs(message,"assistant") ||
      (typeIs(message,"user") && content.is_array() && !content.empty() && typeIs(content[0],"tool_result"));
    if(stoppingPoint && !pendingAttachments.empty()) {
      for(const auto* attachment:pendingAttachments) result.push_back(attachment);
      result.push_back(&message);pendingAttachments.clear();
    } else result.push_back(&message);
  }
  for(const auto* attachment:pendingAttachments) result.push_back(attachment);
  std::reverse(result.begin(),result.end());
  std::erase_if(result,[](const Json* message){
    return (typeIs(*message,"user") || typeIs(*message,"assistant")) && value::truthy(get(*message,"isVirtual"));
  });
  return result;
}
// The normalized form of one tool_use block.
Json normalizedGadgetUse(const Json& block,const Json& gadgets,bool gadgetSearch,const Dependencies& deps,Json* reuse) {
  const Json* found=nullptr;
  for(const auto& gadget:gadgets) {
    need(static_cast<bool>(deps.gadgetMatchesName),"toolMatchesName");
    if(deps.gadgetMatchesName(gadget,get(block,"name").get<std::string>())) {found=&gadget;break;}
  }
  std::string canonical=get(block,"name").get<std::string>();
  Json input;
  if(found) {
    need(static_cast<bool>(deps.normalizeGadgetInputForAPI),"normalizeToolInputForAPI");
    input=deps.normalizeGadgetInputForAPI(*found,get(block,"input"));
    canonical=get(*found,"name").get<std::string>();
  } else input=get(block,"input");
  if(gadgetSearch) {
    Json copy=reuse?std::move(*reuse):block;
    copy["name"]=canonical;copy["input"]=std::move(input);return copy;
  }
  Json copy={{"type","tool_use"}};
  if(has(block,"id")) copy["id"]=block["id"];
  copy["name"]=canonical;copy["input"]=std::move(input);return copy;
}
} // namespace

Json normalizeMessagesForAPI(const Json& messages,const Json& gadgets,const Dependencies& deps) {
  std::set<std::string> availableGadgetNames;
  for(const auto& gadget:gadgets) availableGadgetNames.insert(get(gadget,"name").get<std::string>());
  const auto reordered=reorderedForAPI(messages);

  std::map<std::string,std::set<std::string>> stripTargets;
  for(std::size_t i=0;i<reordered.size();++i) {
    const Json& message=*reordered[i];
    if(!syntheticApiError(message,deps)) continue;
    const Json& content=get(get(message,"message"),"content");
    if(!content.is_array() || content.empty() || !typeIs(content[0],"text")) continue;
    const Json& errorText=get(content[0],"text");
    if(!value::truthy(errorText)) continue;
    const std::string error=errorText.get<std::string>();
    if(!deps.mediaErrorMessages.is_object() || !deps.mediaErrorMessages.contains(error)) continue;
    for(int j=static_cast<int>(i)-1;j>=0;--j) {
      const Json& candidate=*reordered[static_cast<std::size_t>(j)];
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
  for(const auto* entry:reordered) {
    const Json& message=*entry;
    if(typeIs(message,"progress") || (typeIs(message,"system") && get(message,"subtype")!="local_command") ||
       syntheticApiError(message,deps)) continue;
    if(typeIs(message,"system")) {
      need(deps.messageServices!=nullptr,"messageServices");
      Json args={{"content",get(message,"content")}};
      if(has(message,"uuid")) args["uuid"]=message["uuid"];
      if(has(message,"timestamp")) args["timestamp"]=message["timestamp"];
      Json user=messages::createUserMessage(args,*deps.messageServices);
      if(!result.empty() && typeIs(result.back(),"user")) result.back()=mergeUserMessages(std::move(result.back()),user);
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
      const Json& content=get(get(normalized,"message"),"content");
      if(content.is_array()) {
        bool hasBoundary=false;
        for(const auto& block:content) if(typeIs(block,"text") && get(block,"text").is_string() &&
          get(block,"text").get<std::string>().rfind(deps.gadgetReferenceTurnBoundary,0)==0) hasBoundary=true;
        if(!hasBoundary && contentHasGadgetReference(content))
          normalized["message"]["content"].push_back(textBlock(deps.gadgetReferenceTurnBoundary));
      }
      if(!result.empty() && typeIs(result.back(),"user")) result.back()=mergeUserMessages(std::move(result.back()),normalized);
      else result.push_back(std::move(normalized));
      continue;
    }
    if(typeIs(message,"assistant")) {
      need(static_cast<bool>(deps.gadgetSearchEnabledOptimistic),"toolSearchEnabledOptimistic");
      const bool gadgetSearch=deps.gadgetSearchEnabledOptimistic();
      const Json& source=get(get(message,"message"),"content");
      Json normalized=message;
      if(source.is_array()) {
        // The copy already holds every block; only tool_use blocks change.
        Json& content=normalized["message"]["content"];
        for(std::size_t k=0;k<source.size();++k)
          if(typeIs(source[k],"tool_use"))
            content[k]=normalizedGadgetUse(source[k],gadgets,gadgetSearch,deps,&content[k]);
      } else {
        Json mapped=Json::array();
        for(const auto& block:source) {
          if(!typeIs(block,"tool_use")) {mapped.push_back(block);continue;}
          mapped.push_back(normalizedGadgetUse(block,gadgets,gadgetSearch,deps,nullptr));
        }
        normalized["message"]["content"]=std::move(mapped);
      }
      bool merged=false;
      for(int i=static_cast<int>(result.size())-1;i>=0;--i) {
        const Json& previous=result[static_cast<std::size_t>(i)];
        if(!typeIs(previous,"assistant") && !gadgetResultMessage(previous)) break;
        if(typeIs(previous,"assistant") && get(get(previous,"message"),"id")==get(get(normalized,"message"),"id")) {
          result[static_cast<std::size_t>(i)]=mergeAssistantMessages(std::move(result[static_cast<std::size_t>(i)]),normalized);merged=true;break;
        }
      }
      if(!merged) result.push_back(std::move(normalized));
      continue;
    }
    if(typeIs(message,"attachment")) {
      need(static_cast<bool>(deps.normalizeAttachmentForAPI),"normalizeAttachmentForAPI");
      Json attachmentMessages=deps.normalizeAttachmentForAPI(get(message,"attachment"));
      if(!result.empty() && typeIs(result.back(),"user")) {
        Json merged=std::move(result.back());
        for(const auto& attachment:attachmentMessages) merged=mergeUserMessagesAndGadgetResults(std::move(merged),attachment);
        result.back()=std::move(merged);
      } else for(auto& attachment:attachmentMessages) result.push_back(std::move(attachment));
    }
  }
  Json withFilteredOrphans=filterOrphanedThinkingOnlyMessages(std::move(result));
  Json withFilteredThinking=filterTrailingThinkingFromLastAssistant(std::move(withFilteredOrphans));
  Json withFilteredWhitespace=filterWhitespaceOnlyAssistantMessages(std::move(withFilteredThinking));
  Json withNonEmpty=ensureNonEmptyAssistantContent(std::move(withFilteredWhitespace),deps);
  Json sanitized=sanitizeErrorGadgetResultContent(std::move(withNonEmpty));
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
    for(auto& message:normalized) {
      if(typeIs(message,"user")) mapped.push_back(stripGadgetReferenceBlocksFromUserMessage(std::move(message)));
      else if(typeIs(message,"assistant")) mapped.push_back(stripCallerFieldFromAssistantMessage(std::move(message)));
      else mapped.push_back(std::move(message));
    }
    normalized=std::move(mapped);
  }
  need(deps.messageServices!=nullptr,"messageServices");
  normalized=messages::ensureGadgetResultPairing(std::move(normalized),*deps.messageServices);
  if(!advisorBetaEnabled) normalized=stripAdvisorBlocks(std::move(normalized));
  return requests::stripExcessMediaItems(std::move(normalized),100);
}

} // namespace batbox::normalization

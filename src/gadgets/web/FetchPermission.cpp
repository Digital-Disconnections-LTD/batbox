#include "gadgets/web/Web.hpp"
#include <stdexcept>

namespace batbox::gadgets::web {
std::string webFetchPermissionRuleContent(const Json& input){
  if(!input.is_object()||!input.contains("url")||!input.at("url").is_string()||
      !input.contains("prompt")||!input.at("prompt").is_string())
    return "input:[object Object]";
  const auto host=hostnameFromURL(input.at("url").get<std::string>());
  return host?"domain:"+*host:"input:[object Object]";
}

Json checkFetchPermissions(const Json& input,const Json& context,const FetchServices& services){
  const auto url=input.value("url",std::string());
  if(isPreapprovedUrl(url,services.preapprovedHosts))
    return {{"behavior","allow"},{"updatedInput",input},
      {"decisionReason",{{"type","other"},{"reason","Preapproved host"}}}};
  if(!services.permissionRule)throw std::invalid_argument("Missing WebFetch permissionRule binding");
  const auto content=webFetchPermissionRuleContent(input);
  auto suggestions=[&](){return Json::array({{{"type","addRules"},
    {"destination","localSettings"},
    {"rules",Json::array({{{"toolName","WebFetch"},{"ruleContent",content}}})},
    {"behavior","allow"}}});};
  if(const auto deny=services.permissionRule(context,"deny",content))
    return {{"behavior","deny"},{"message","WebFetch denied access to "+content+"."},
      {"decisionReason",{{"type","rule"},{"rule",*deny}}}};
  if(const auto ask=services.permissionRule(context,"ask",content))
    return {{"behavior","ask"},
      {"message","Batbox requested permissions to use WebFetch, but you haven't granted it yet."},
      {"decisionReason",{{"type","rule"},{"rule",*ask}}},
      {"suggestions",suggestions()}};
  if(const auto allow=services.permissionRule(context,"allow",content))
    return {{"behavior","allow"},{"updatedInput",input},
      {"decisionReason",{{"type","rule"},{"rule",*allow}}}};
  return {{"behavior","ask"},
    {"message","Batbox requested permissions to use WebFetch, but you haven't granted it yet."},
    {"suggestions",suggestions()}};
}
} // namespace batbox::gadgets::web

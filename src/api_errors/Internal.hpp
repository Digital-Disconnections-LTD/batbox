#pragma once
#include "api_errors/ApiErrors.hpp"
#include "runtime/JsonValue.hpp"
#include "utils/envUtils.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace batbox::api_errors::internal {
inline const std::string API_ERROR_MESSAGE_PREFIX="API Error";
inline const std::string POETRY_TOO_LONG_ERROR_MESSAGE="Prompt is too long";
inline const std::string CREDIT_BALANCE_TOO_LOW_ERROR_MESSAGE="Credit balance is too low";
inline const std::string INVALID_API_KEY_ERROR_MESSAGE="Not logged in · Please run /login";
inline const std::string INVALID_API_KEY_ERROR_MESSAGE_EXTERNAL="Invalid API key · Fix external API key";
inline const std::string ORG_DISABLED_ERROR_MESSAGE_ENV_KEY_WITH_OAUTH="Your BATBOX_API_KEY belongs to a disabled organization · Unset the environment variable to use your subscription instead";
inline const std::string ORG_DISABLED_ERROR_MESSAGE_ENV_KEY="Your BATBOX_API_KEY belongs to a disabled organization · Update or unset the environment variable";
inline const std::string TOKEN_REVOKED_ERROR_MESSAGE="OAuth token revoked · Please run /login";
inline const std::string CCR_AUTH_ERROR_MESSAGE="Authentication error · This may be a temporary network issue, please try again";
inline const std::string CUSTOM_OFF_SWITCH_MESSAGE="Opus is experiencing high load, please use /model to switch to Sonnet";
inline const std::string API_TIMEOUT_ERROR_MESSAGE="Request timed out";
inline const std::string OAUTH_ORG_NOT_ALLOWED_ERROR_MESSAGE="Your account does not have access to Batbox. Please run /login.";
bool isCCRMode(Providers& p);
std::string getPdfTooLargeErrorMessage(Providers& p);
std::string getPdfPasswordProtectedErrorMessage(Providers& p);
std::string getPdfInvalidErrorMessage(Providers& p);
std::string getImageTooLargeErrorMessage(Providers& p);
std::string getRequestTooLargeErrorMessage(Providers& p);
std::string getTokenRevokedErrorMessage(Providers& p);
std::string getOauthOrgNotAllowedErrorMessage(Providers& p);
std::optional<std::string> get3PModelFallbackSuggestion(const std::string& model,Providers& p);
inline bool has(const Json& value,const std::string& key){return value.is_object()&&value.contains(key);}
inline Json get(const Json& value,const std::string& key){return has(value,key)?value.at(key):Json();}
inline std::string string(const Json& value){return value.is_string()?value.get<std::string>():std::string();}
inline std::string field(const Json& value,const std::string& key){return string(get(value,key));}
inline bool contains(const std::string& text,const std::string& needle){return text.find(needle)!=std::string::npos;}
inline bool starts(const std::string& text,const std::string& prefix){return text.rfind(prefix,0)==0;}
inline std::string lower(std::string text){for(char& c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return text;}
inline Json envValue(const Providers& p,const std::string& key){return has(p.env,key)?p.env.at(key):Json();}
inline bool envTruthy(const Providers& p,const std::string& key){return batbox::isEnvTruthy(envValue(p,key));}
inline bool envPresent(const Providers& p,const std::string& key){return value::truthy(envValue(p,key));}
inline std::optional<std::string> header(const retry::ErrorRecord& e,const std::string& name){
  if(name.empty())throw std::logic_error("WireHeaders value not supplied");
  if(!e.headers.is_object())return std::nullopt;
  const auto search=lower(name);
  for(auto it=e.headers.begin();it!=e.headers.end();++it)if(lower(it.key())==search&&it.value().is_string())return it.value().get<std::string>();
  return std::nullopt;
}
template<class Fn>inline void require(const Fn& callback,const char* name){if(!callback)throw std::logic_error(std::string(name)+" dependency not supplied");}
inline bool nonInteractive(Providers& p){require(p.nonInteractive,"nonInteractive");return p.nonInteractive();}
inline Json assistantError(const Json& args,Providers& p){
  if(!p.messageServices)throw std::logic_error("messageServices dependency not supplied");
  return messages::createAssistantAPIErrorMessage(args,*p.messageServices);
}
inline Json assistantError(const std::string& content,const std::optional<std::string>& kind,Providers& p,
    const std::optional<std::string>& details=std::nullopt){
  Json args={{"content",content}};
  if(kind)args["error"]=*kind;
  if(details)args["errorDetails"]=*details;
  return assistantError(args,p);
}
inline std::string required(const std::string& value,const char* name){if(value.empty())throw std::logic_error(std::string(name)+" configuration not supplied");return value;}
} // namespace batbox::api_errors::internal

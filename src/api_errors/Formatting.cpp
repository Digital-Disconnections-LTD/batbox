#include "api_errors/Internal.hpp"
#include <regex>

namespace batbox::api_errors {
namespace {
std::string sanitizeMessageHTML(const std::string& message){
  if(internal::contains(message,"<!DOCTYPE html")||internal::contains(message,"<html")){
    static const std::regex title(R"(<title>([^<]+)</title>)");
    std::smatch match;
    if(std::regex_search(message,match,title)&&match.size()>1&&!match[1].str().empty())
      return value::trim(match[1].str());
    return "";
  }
  return message;
}
bool hasNestedError(const Json& value){
  return value.is_object()||value.is_array();
}
std::optional<std::string> extractNestedErrorMessage(const retry::ErrorRecord& error){
  if(!hasNestedError(error.body))return std::nullopt;
  const Json nested=error.body;
  const Json deep=internal::get(internal::get(nested,"error"),"message");
  if(deep.is_string()&&!deep.get<std::string>().empty()){
    const auto sanitized=sanitizeMessageHTML(deep.get<std::string>());
    if(!sanitized.empty())return sanitized;
  }
  const Json shallow=internal::get(nested,"message");
  if(shallow.is_string()&&!shallow.get<std::string>().empty()){
    const auto sanitized=sanitizeMessageHTML(shallow.get<std::string>());
    if(!sanitized.empty())return sanitized;
  }
  return std::nullopt;
}
std::string sanitizeAPIError(const retry::ErrorRecord& error){
  if(error.message.empty())return "";
  return sanitizeMessageHTML(error.message);
}
}
std::string formatAPIError(const retry::ErrorRecord& error){
  const auto connectionDetails=retry::extractConnectionErrorDetails(error);
  if(connectionDetails){
    const auto& code=connectionDetails->code;
    if(code=="ETIMEDOUT")return "Request timed out. Check your internet connection and proxy settings";
    if(connectionDetails->isSSLError){
      if(code=="UNABLE_TO_VERIFY_LEAF_SIGNATURE"||code=="UNABLE_TO_GET_ISSUER_CERT"||
         code=="UNABLE_TO_GET_ISSUER_CERT_LOCALLY")
        return "Unable to connect to API: SSL certificate verification failed. Check your proxy or corporate SSL certificates";
      if(code=="CERT_HAS_EXPIRED")return "Unable to connect to API: SSL certificate has expired";
      if(code=="CERT_REVOKED")return "Unable to connect to API: SSL certificate has been revoked";
      if(code=="DEPTH_ZERO_SELF_SIGNED_CERT"||code=="SELF_SIGNED_CERT_IN_CHAIN")
        return "Unable to connect to API: Self-signed certificate detected. Check your proxy or corporate SSL certificates";
      if(code=="ERR_TLS_CERT_ALTNAME_INVALID"||code=="HOSTNAME_MISMATCH")
        return "Unable to connect to API: SSL certificate hostname mismatch";
      if(code=="CERT_NOT_YET_VALID")return "Unable to connect to API: SSL certificate is not yet valid";
      return "Unable to connect to API: SSL error ("+code+")";
    }
  }
  if(error.message=="Connection error."){
    if(connectionDetails&&!connectionDetails->code.empty())return "Unable to connect to API ("+connectionDetails->code+")";
    return "Unable to connect to API. Check your internet connection";
  }
  if(error.message.empty()){
    const auto nested=extractNestedErrorMessage(error);
    if(nested)return *nested;
    return "API error (status "+(error.status?std::to_string(*error.status):"unknown")+")";
  }
  const auto sanitized=sanitizeAPIError(error);
  return sanitized!=error.message&&!sanitized.empty()?sanitized:error.message;
}
} // namespace batbox::api_errors

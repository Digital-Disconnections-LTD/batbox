#include "api_errors/Internal.hpp"
#include "utils/messages.hpp"
#include <cmath>
#include <regex>

namespace batbox::api_errors {
namespace {
std::optional<double> numberHeader(const std::optional<std::string>& value){
  if(!value||value->empty())return std::nullopt;
  try{std::size_t end=0;const std::string trimmed=value::trim(*value);
    double result=std::stod(trimmed,&end);
    if(end==trimmed.size())return result;
  }catch(...){}
  return std::nullopt;
}
std::string fallbackCommand(Providers& p){return internal::nonInteractive(p)?"--model":"/model";}
}
Json getAssistantMessageFromError(const Failure& failure,const std::string& model,Providers& p){
  const auto& error=failure.error;
  const std::string& message=error.message;
  const bool api=error.isAPIError();
  const bool connection=error.isConnectionError();
  const bool isError=failure.isErrorObject;
  if(failure.isConnectionTimeout||(connection&&internal::contains(internal::lower(message),"timeout")))
    return internal::assistantError(internal::API_TIMEOUT_ERROR_MESSAGE,"unknown",p);
  if(failure.isImageSize||failure.isImageResize)
    return internal::assistantError(internal::getImageTooLargeErrorMessage(p),std::nullopt,p);
  if(isError&&internal::contains(message,internal::CUSTOM_OFF_SWITCH_MESSAGE))
    return internal::assistantError(internal::CUSTOM_OFF_SWITCH_MESSAGE,"rate_limit",p);
  if(api&&error.status==429){
    internal::require(p.subscriber,"subscriber");
    internal::require(p.shouldProcessRateLimits,"shouldProcessRateLimits");
    if(p.shouldProcessRateLimits(p.subscriber())){
      const auto rateLimitType=internal::header(error,p.wire.representativeClaim);
      const auto overageStatus=internal::header(error,p.wire.overageStatus);
      if((rateLimitType&&!rateLimitType->empty())||(overageStatus&&!overageStatus->empty())){
        Json limits={{"status","rejected"},{"unifiedRateLimitFallbackAvailable",false},{"isUsingOverage",false}};
        const auto resetHeader=internal::header(error,p.wire.reset);
        if(resetHeader&&!resetHeader->empty()){
          auto reset=numberHeader(resetHeader);limits["resetsAt"]=reset?Json(*reset):Json(nullptr);
        }
        if(rateLimitType&&!rateLimitType->empty())limits["rateLimitType"]=*rateLimitType;
        if(overageStatus&&!overageStatus->empty())limits["overageStatus"]=*overageStatus;
        const auto overageReset=internal::header(error,p.wire.overageReset);
        if(overageReset&&!overageReset->empty()){
          auto reset=numberHeader(overageReset);limits["overageResetsAt"]=reset?Json(*reset):Json(nullptr);
        }
        const auto disabled=internal::header(error,p.wire.overageDisabledReason);
        if(disabled&&!disabled->empty())limits["overageDisabledReason"]=*disabled;
        internal::require(p.rateLimitErrorMessage,"rateLimitErrorMessage");
        const auto specific=p.rateLimitErrorMessage(limits,model);
        if(specific&&!specific->empty())return internal::assistantError(*specific,"rate_limit",p);
        return internal::assistantError(NO_RESPONSE_REQUESTED,"rate_limit",p);
      }
      if(internal::contains(message,"Extra usage is required for long context")){
        const std::string hint=internal::nonInteractive(p)
          ? "enable extra usage at "+internal::required(p.usageURL,"usageURL")+", or use --model to switch to standard context"
          : "run /extra-usage to enable, or /model to switch to standard context";
        return internal::assistantError(internal::API_ERROR_MESSAGE_PREFIX+": Extra usage is required for 1M context · "+hint,"rate_limit",p);
      }
      static const std::regex prefix(R"(^429\s+)");
      static const std::regex inner(R"re("message"\s*:\s*"([^"]*)")re");
      const std::string stripped=std::regex_replace(message,prefix,"");
      std::smatch match;
      const std::string innerMessage=std::regex_search(stripped,match,inner)?match[1].str():"";
      const std::string detail=!innerMessage.empty()?innerMessage:stripped;
      const std::string shown=!detail.empty()?detail:
        "this may be a temporary capacity issue — check "+internal::required(p.statusURL,"statusURL");
      return internal::assistantError(internal::API_ERROR_MESSAGE_PREFIX+": Request rejected (429) · "+shown,"rate_limit",p);
    }
  }
  if(isError&&internal::contains(internal::lower(message),"prompt is too long"))
    return internal::assistantError(internal::POETRY_TOO_LONG_ERROR_MESSAGE,"invalid_request",p,message);
  static const std::regex pdfLimit(R"(maximum of \d+ PDF pages)");
  if(isError&&std::regex_search(message,pdfLimit))
    return internal::assistantError(internal::getPdfTooLargeErrorMessage(p),"invalid_request",p,message);
  if(isError&&internal::contains(message,"The PDF specified is password protected"))
    return internal::assistantError(internal::getPdfPasswordProtectedErrorMessage(p),"invalid_request",p);
  if(isError&&internal::contains(message,"The PDF specified was not valid"))
    return internal::assistantError(internal::getPdfInvalidErrorMessage(p),"invalid_request",p);
  if(api&&error.status==400&&internal::contains(message,"image exceeds")&&internal::contains(message,"maximum"))
    return internal::assistantError(internal::getImageTooLargeErrorMessage(p),std::nullopt,p,message);
  if(api&&error.status==400&&internal::contains(message,"image dimensions exceed")&&internal::contains(message,"many-image")){
    const std::string text=internal::nonInteractive(p)
      ? "An image in the conversation exceeds the dimension limit for many-image requests (2000px). Start a new session with fewer images."
      : "An image in the conversation exceeds the dimension limit for many-image requests (2000px). Run /compact to remove old images from context, or start a new session.";
    return internal::assistantError(text,"invalid_request",p,message);
  }
  if(!p.betaHeaderValue.empty()&&api&&error.status==400&&
     internal::contains(message,p.betaHeaderValue)&&
     internal::contains(message,internal::required(p.wire.betaHeaderName,"betaHeaderName")))
    return internal::assistantError("Auto mode is unavailable for your plan","invalid_request",p);
  if(api&&error.status==413)
    return internal::assistantError(internal::getRequestTooLargeErrorMessage(p),"invalid_request",p);
  if(api&&error.status==400&&internal::contains(message,"`tool_use` ids were found without `tool_result` blocks immediately after")){
    const std::string rewind=internal::nonInteractive(p)?"":" Run /rewind to recover the conversation.";
    return internal::assistantError("API Error: 400 due to tool use concurrency issues."+rewind,"invalid_request",p);
  }
  // Unexpected Gadget results do not emit analytics.
  if(api&&error.status==400&&internal::contains(message,"`tool_use` ids must be unique")){
    const std::string rewind=internal::nonInteractive(p)?"":" Run /rewind to recover the conversation.";
    return internal::assistantError("API Error: 400 duplicate tool_use ID in conversation history."+rewind,"invalid_request",p,message);
  }
  internal::require(p.subscriber,"subscriber");
  if(p.subscriber()&&api&&error.status==400&&internal::contains(internal::lower(message),"invalid model name")){
    internal::require(p.isDefaultPremiumModel,"isDefaultPremiumModel");
    if(p.isDefaultPremiumModel(model)||model=="opus")
      return internal::assistantError("Opus is not available with your current plan. If you have updated your subscription plan recently, run /logout and /login for the plan to take effect.","invalid_request",p);
  }
  // Model-gating guidance and feedback collection are not enabled here.
  if(isError&&internal::contains(message,"Your credit balance is too low"))
    return internal::assistantError(internal::CREDIT_BALANCE_TOO_LOW_ERROR_MESSAGE,"billing_error",p);
  if(api&&error.status==400&&internal::contains(internal::lower(message),"organization has been disabled")){
    internal::require(p.apiKeySource,"apiKeySource");
    const std::string source=p.apiKeySource();
    if(source=="BATBOX_API_KEY"&&internal::envPresent(p,"BATBOX_API_KEY")&&!p.subscriber()){
      internal::require(p.oauthAccessToken,"oauthAccessToken");
      const bool stored=p.oauthAccessToken().has_value();
      return internal::assistantError(stored?internal::ORG_DISABLED_ERROR_MESSAGE_ENV_KEY_WITH_OAUTH:
        internal::ORG_DISABLED_ERROR_MESSAGE_ENV_KEY,"invalid_request",p);
    }
  }
  if(isError&&internal::contains(internal::lower(message),"x-api-key")){
    if(internal::isCCRMode(p))
      return internal::assistantError(internal::CCR_AUTH_ERROR_MESSAGE,"authentication_failed",p);
    internal::require(p.apiKeySource,"apiKeySource");
    const std::string source=p.apiKeySource();
    const bool external=source=="BATBOX_API_KEY"||source=="apiKeyHelper";
    return internal::assistantError(external?internal::INVALID_API_KEY_ERROR_MESSAGE_EXTERNAL:
      internal::INVALID_API_KEY_ERROR_MESSAGE,"authentication_failed",p);
  }
  if(api&&error.status==403&&internal::contains(message,"OAuth token has been revoked"))
    return internal::assistantError(internal::getTokenRevokedErrorMessage(p),"authentication_failed",p);
  if(api&&(error.status==401||error.status==403)&&
     internal::contains(message,"OAuth authentication is currently not allowed for this organization"))
    return internal::assistantError(internal::getOauthOrgNotAllowedErrorMessage(p),"authentication_failed",p);
  if(api&&(error.status==401||error.status==403)){
    if(internal::isCCRMode(p))
      return internal::assistantError(internal::CCR_AUTH_ERROR_MESSAGE,"authentication_failed",p);
    const std::string text=internal::nonInteractive(p)
      ? "Failed to authenticate. API Error: "+message
      : "Please run /login · API Error: "+message;
    return internal::assistantError(text,"authentication_failed",p);
  }
  if(internal::envTruthy(p,"BATBOX_USE_BEDROCK")&&isError&&
     internal::contains(internal::lower(message),"model id")){
    const std::string switchCmd=fallbackCommand(p);
    const auto suggestion=internal::get3PModelFallbackSuggestion(model,p);
    const std::string text=suggestion
      ? "API Error ("+model+"): "+message+". Try "+switchCmd+" to switch to "+*suggestion+"."
      : "API Error ("+model+"): "+message+". Run "+switchCmd+" to pick a different model.";
    return internal::assistantError(text,"invalid_request",p);
  }
  if(api&&error.status==404){
    const std::string switchCmd=fallbackCommand(p);
    const auto suggestion=internal::get3PModelFallbackSuggestion(model,p);
    std::string text;
    if(suggestion){
      internal::require(p.apiProvider,"apiProvider");
      text="The model "+model+" is not available on your "+p.apiProvider()+" deployment. Try "+switchCmd+
        " to switch to "+*suggestion+", or ask your admin to enable this model.";
    }else text="There's an issue with the selected model ("+model+"). It may not exist or you may not have access to it. Run "+switchCmd+" to pick a different model.";
    return internal::assistantError(text,"invalid_request",p);
  }
  if(connection)return internal::assistantError("API Error: "+formatAPIError(error),"unknown",p);
  if(isError)return internal::assistantError("API Error: "+message,"unknown",p);
  return internal::assistantError(internal::API_ERROR_MESSAGE_PREFIX,"unknown",p);
}
} // namespace batbox::api_errors

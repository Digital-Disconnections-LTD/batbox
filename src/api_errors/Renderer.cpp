#include "api_errors/Internal.hpp"
#include "utils/messages.hpp"
#include <cmath>
#include <regex>

namespace batbox::api_errors {
namespace {
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
  if(!p.betas.empty()&&api&&error.status==400&&
     internal::contains(message,internal::required(p.wire.betaHeaderName,"betaHeaderName"))){
    std::vector<std::string> named;
    for(const auto& beta:p.betas)if(internal::contains(message,beta))named.push_back(beta);
    const auto& shown=named.empty()?p.betas:named;
    std::string list;
    for(const auto& beta:shown)list+=(list.empty()?"":", ")+beta;
    const std::string text=named.empty()
      ? "API Error: 400 · The server rejected the "+p.wire.betaHeaderName+" header ("+list+
        ") · Check this endpoint's \"betas\" setting"
      : "API Error: 400 · The server does not accept "+p.wire.betaHeaderName+" "+list+
        " · Remove "+(named.size()==1?"it":"them")+" from this endpoint's \"betas\" setting";
    return internal::assistantError(text,"invalid_request",p,message);
  }
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
  // Model-gating guidance and feedback collection are not enabled here.
  if(isError&&internal::contains(message,"Your credit balance is too low"))
    return internal::assistantError(internal::CREDIT_BALANCE_TOO_LOW_ERROR_MESSAGE,"billing_error",p);
  if(isError&&internal::contains(internal::lower(message),"x-api-key"))
    return internal::assistantError(internal::INVALID_API_KEY_ERROR_MESSAGE,
      "authentication_failed",p);
  if(api&&(error.status==401||error.status==403)){
    return internal::assistantError("Failed to authenticate. API Error: "+message,"authentication_failed",p);
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

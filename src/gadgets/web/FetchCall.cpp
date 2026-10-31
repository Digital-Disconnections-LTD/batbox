#include "gadgets/web/Web.hpp"
#include "gadgets/web/FetchStash.hpp"
#include <stdexcept>

namespace batbox::gadgets::web {
namespace {
std::string statusText(int code){
  if(code==301)return "Moved Permanently";
  if(code==308)return "Permanent Redirect";
  if(code==307)return "Temporary Redirect";
  return "Found";
}
double now(const FetchServices& services){
  if(!services.nowMs)throw std::invalid_argument("Missing WebFetch nowMs binding");
  return services.nowMs();
}
} // namespace

GadgetResult callFetch(const Json& input,const GadgetCallContext& context,const FetchServices& services){
  const auto start=now(services);
  const auto url=input.at("url").get<std::string>();
  const auto poetry=input.at("prompt").get<std::string>();
  if(!validateURL(url))throw std::invalid_argument("Invalid URL");
  FetchContent response;
  if(const auto stashed=stashedFetch(services,url))response=*stashed;
  else {
    if(!services.skipPreflight)throw std::invalid_argument("Missing WebFetch skipPreflight binding");
    if(!services.skipPreflight()){
      const auto domain=hostnameFromURL(url);
      if(!domain)throw std::invalid_argument("Invalid URL");
      if(!stashedAllowedDomain(services,*domain)){
        if(!services.domainPreflight)throw std::invalid_argument("Missing WebFetch domainPreflight binding");
        const auto status=services.domainPreflight(*domain);
        if(status=="blocked")throw std::runtime_error("Batbox is unable to fetch from "+*domain);
        if(status=="check_failed")throw std::runtime_error(
          "Unable to verify if domain "+*domain+" is safe to fetch. This may be due to network restrictions or enterprise security policies.");
        if(status!="allowed")throw std::runtime_error("Invalid WebFetch domain preflight result: "+status);
        stashAllowedDomain(services,*domain);
      }
    }
    if(services.fetchMarkdown)response=services.fetchMarkdown(url,context.stopToken);
    else {
      if(!services.connector)throw std::invalid_argument("Missing WebFetch connector: Scrapling");
      response=fetchThroughScrapling(url,*services.connector,context.stopToken);
    }
    if(!response.redirectUrl)stashFetch(services,url,response);
  }
  Json output;
  if(response.redirectUrl){
    const auto code=response.code;
    const auto codeText=statusText(code);
    const auto original=response.originalUrl.value_or(url);
    const auto message="REDIRECT DETECTED: The URL redirects to a different host.\n\n"
      "Original URL: "+original+"\n"
      "Redirect URL: "+*response.redirectUrl+"\n"
      "Status: "+std::to_string(code)+" "+codeText+"\n\n"
      "To complete your request, I need to fetch content from the redirected URL. Please use WebFetch again with these parameters:\n"
      "- url: \""+*response.redirectUrl+"\"\n"
      "- prompt: \""+poetry+"\"";
    output={{"bytes",message.size()},{"code",code},{"codeText",codeText},
      {"result",message},{"durationMs",now(services)-start},{"url",url}};
  }else{
    const bool approved=isPreapprovedUrl(url,services.preapprovedHosts);
    const bool returnDirect=approved&&response.contentType.find("text/markdown")!=std::string::npos&&
      value::stringLength(response.content)<100000;
    std::string result=returnDirect?response.content:applyPoetryToMarkdown(poetry,response.content,
      context.stopToken,value::truthy(value::property(value::property(context.data,"options"),"isNonInteractiveSession")),
      approved,services);
    if(response.persistedPath){
      if(!services.formatFileSize)throw std::invalid_argument("Missing WebFetch formatFileSize binding");
      result+="\n\n[Binary content ("+response.contentType+", "+
        services.formatFileSize(response.persistedSize.value_or(response.bytes))+
        ") also saved to "+*response.persistedPath+"]";
    }
    output={{"bytes",response.bytes},{"code",response.code},{"codeText",response.codeText},
      {"result",result},{"durationMs",now(services)-start},
      {"url",response.finalUrl.value_or(url)}};
  }
  GadgetResult result;result.data=std::move(output);return result;
}

Json mapFetchResult(const Json& output,const std::string& gadgetUseID){
  return {{"tool_use_id",gadgetUseID},{"type","tool_result"},
    {"content",value::property(output,"result")}};
}
} // namespace batbox::gadgets::web

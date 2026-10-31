#include "gadgets/web/Web.hpp"
#include "connectors/Abort.hpp"
#include <stdexcept>

namespace batbox::gadgets::web {
namespace {
std::string statusText(int code){
  switch(code){
    case 200:return "OK";case 201:return "Created";case 202:return "Accepted";
    case 204:return "No Content";case 301:return "Moved Permanently";
    case 302:return "Found";case 307:return "Temporary Redirect";
    case 308:return "Permanent Redirect";case 400:return "Bad Request";
    case 401:return "Unauthorized";case 403:return "Forbidden";
    case 404:return "Not Found";case 429:return "Too Many Requests";
    case 500:return "Internal Server Error";case 502:return "Bad Gateway";
    case 503:return "Service Unavailable";case 504:return "Gateway Timeout";
    default:return "";
  }
}
std::string upgraded(const std::string& url){
  return url.rfind("http://",0)==0?"https://"+url.substr(7):url;
}
} // namespace

FetchContent fetchThroughScrapling(const std::string& url,Scrapling& connector,std::stop_token stopToken){
  checkConnectorAbort(stopToken);
  const auto response=connector.fetch(upgraded(url),stopToken);
  checkConnectorAbort(stopToken);
  if(response.value("isError",false))throw std::runtime_error("Scrapling fetch failed: "+value::stringify(response));
  const auto& blocks=response.at("content");
  if(!blocks.is_array()||blocks.empty())throw std::runtime_error("Scrapling returned no page response");
  const auto text=blocks.front().at("text").get<std::string>();
  const auto page=value::parse(text);
  if(!page.is_object()||!page.contains("status")||!page.contains("content")||!page.contains("url"))
    throw std::runtime_error("Scrapling page response is incomplete");
  FetchContent output;
  output.code=page.at("status").get<int>();
  output.codeText=statusText(output.code);
  const auto pieces=page.at("content");
  if(!pieces.is_array())throw std::runtime_error("Scrapling page content is not an array");
  for(const auto& piece:pieces){
    if(!output.content.empty())output.content+='\n';
    output.content+=piece.get<std::string>();
  }
  output.bytes=output.content.size();
  output.contentType="text/markdown";
  output.finalUrl=page.at("url").get<std::string>();
  return output;
}
} // namespace batbox::gadgets::web

#include "providers/Onboarding.hpp"
#include "application_config/EndpointStore.hpp"
#ifdef _WIN32
#include "runtime/IndigenousUnicode.hpp"
#endif
#include <algorithm>
#include <cstdlib>
#include <set>
#include <stdexcept>

namespace batbox::providers {
namespace {
std::string origin(const std::string& url){
  const auto scheme=url.find("://");
  if(scheme==std::string::npos)return url;
  const auto end=url.find('/',scheme+3);
  return url.substr(scheme+3,end==std::string::npos?std::string::npos:end-scheme-3);
}
bool loopback(const std::string& url){
  const auto host=origin(url);
  return host.starts_with("127.0.0.1")||host.starts_with("localhost")||host.starts_with("[::1]");
}
}

Environment processEnvironment(){
  return [](const std::string& name)->std::optional<std::string>{
#ifdef _WIN32
    auto value=runtime::environmentUtf8(name.c_str());
    if(value.empty())return std::nullopt;
    return value;
#else
    const char* value=std::getenv(name.c_str());
    if(!value||!*value)return std::nullopt;
    return std::string(value);
#endif
  };
}

std::string resolvedBaseUrl(const CatalogProvider& provider,const Environment& environment){
  auto url=provider.baseUrl;
  std::size_t start=0;
  while((start=url.find("${",start))!=std::string::npos){
    const auto end=url.find('}',start);
    if(end==std::string::npos)break;
    const auto name=url.substr(start+2,end-start-2);
    const auto value=environment?environment(name):std::nullopt;
    if(!value){start=end+1;continue;}
    url.replace(start,end-start+1,*value);
    start+=value->size();
  }
  while(url.ends_with('/'))url.pop_back();
  return url;
}

bool needsBaseUrlEdit(const std::string& baseUrl){
  return baseUrl.find('{')!=std::string::npos||baseUrl.find('}')!=std::string::npos;
}

std::vector<std::string> presentKeyEnv(const CatalogProvider& provider,
    const Environment& environment){
  std::vector<std::string> result;
  if(!environment)return result;
  for(const auto& name:provider.keyEnv)if(environment(name))result.push_back(name);
  return result;
}

bool takesKey(const CatalogProvider& provider){
  return provider.auth=="key";
}

application::EndpointRecord buildEndpoint(const CatalogProvider& provider,const Setup& setup,
    std::size_t defaultContext,std::size_t defaultOutput){
  if(!provider.supported())throw std::invalid_argument(provider.name+" is not supported yet: "+
    provider.reason);
  if(setup.model.empty())throw std::invalid_argument("Choose a model");
  application::EndpointRecord endpoint;
  endpoint.label=provider.name;
  endpoint.baseUrl=setup.baseUrl.empty()?provider.baseUrl:setup.baseUrl;
  while(endpoint.baseUrl.ends_with('/'))endpoint.baseUrl.pop_back();
  if(needsBaseUrlEdit(endpoint.baseUrl))
    throw std::invalid_argument("Replace the placeholder in the base URL: "+endpoint.baseUrl);
  endpoint.messagesPath=provider.messagesPath.empty()?"/v1/messages":provider.messagesPath;
  endpoint.modelsPath=provider.modelsPath;
  endpoint.headers=provider.headers.is_object()?provider.headers:Json::object();
  endpoint.provider=provider.kind;endpoint.preset=provider.id;
  if(provider.kind=="openai-chat"&&provider.quirks.is_object())endpoint.quirks=provider.quirks;
  const bool hasKey=!setup.key.empty()||!setup.keyEnv.empty();
  endpoint.authHeader=provider.authHeader;
  if(hasKey&&endpoint.authHeader=="none")endpoint.authHeader="authorization";
  if(!hasKey&&(provider.auth=="none"||loopback(endpoint.baseUrl)))endpoint.authHeader="none";
  if(!setup.key.empty())endpoint.authToken=setup.key;
  else endpoint.apiKeyEnv=setup.keyEnv;
  std::vector<std::string> models{setup.model};
  std::set<std::string> seen{setup.model};
  const auto add=[&](const std::string& id){
    if(models.size()<200&&!id.empty()&&id.find_first_of("\r\n")==std::string::npos&&
        seen.insert(id).second)models.push_back(id);
  };
  for(const auto& id:setup.liveModels)add(id);
  if(setup.liveModels.empty())for(const auto& model:provider.models)add(model.id);
  endpoint.models=std::move(models);
  std::size_t context=0,output=0;
  if(const auto* known=provider.model(setup.model)){context=known->context;output=known->output;}
  if(const auto live=setup.liveContexts.find(setup.model);live!=setup.liveContexts.end()&&
      live->second>0)context=live->second;
  if(context==0)context=defaultContext;
  if(output==0)output=std::min(defaultOutput,context);
  endpoint.contextWindow=context;
  endpoint.maxOutputTokens=std::min(output,context);
  if(provider.id=="anthropic"){
    const auto* known=provider.model(setup.model);
    const bool reasoning=known&&known->reasoning;
    endpoint.thinking=reasoning;endpoint.effort=reasoning;endpoint.poetryStashing=true;
  }
  return endpoint;
}

std::string uniqueEndpointId(const std::map<std::string,application::EndpointRecord>& endpoints,
    const std::string& base){
  if(!endpoints.contains(base))return base;
  for(int suffix=2;;++suffix){
    auto candidate=base+"-"+std::to_string(suffix);
    if(!endpoints.contains(candidate))return candidate;
  }
}

ConnectionResult testConnection(const application::EndpointRecord& endpoint,
    mcp::http::Fetch fetch,std::chrono::milliseconds timeout){
  ConnectionResult result;
  if(endpoint.provider=="bedrock"||endpoint.provider=="vertex"){
    result.message="Bedrock and Vertex sign each request with cloud credentials; "
      "Batbox checks them on the first request.";
    return result;
  }
  mcp::http::Request request;
  try{request.url=application_config::modelsUrl(endpoint);}
  catch(const std::exception& error){result.message=error.what();return result;}
  request.method="GET";request.followRedirects=false;
  request.timeout=timeout;request.connectTimeout=std::min(timeout,
    std::chrono::milliseconds(5000));
  request.headers=endpoint.headers.is_object()?endpoint.headers:Json::object();
  request.headers["accept"]="application/json";
  std::string token=endpoint.authToken;
  if(token.empty()&&!endpoint.apiKeyEnv.empty())
    if(const auto value=processEnvironment()(endpoint.apiKeyEnv))token=*value;
  if(endpoint.authHeader=="authorization"&&!token.empty())
    request.headers["authorization"]="Bearer "+token;
  else if(endpoint.authHeader=="x-api-key"&&!token.empty()){
    request.headers["x-api-key"]=token;
    if(!request.headers.contains("anthropic-version"))
      request.headers["anthropic-version"]="2023-06-01";
  }
  const auto host=origin(request.url);
  mcp::http::Response response;
  try{response=fetch(request);}
  catch(const std::exception&){
    result.message="Could not connect to "+host+(loopback(request.url)?
      ". Is the server running?":". Check the base URL and your network.");
    return result;
  }
  result.reached=true;result.status=response.status;
  if(response.status==401||response.status==403){
    result.message="The server rejected the key (HTTP "+std::to_string(response.status)+
      "). Check the key or its environment variable.";
    return result;
  }
  if(response.status==404){
    result.message="No model list at "+request.url+" (HTTP 404). The server may still "
      "answer requests; pick a model from the catalog.";
    return result;
  }
  if(!response.ok()){
    result.message="HTTP "+std::to_string(response.status)+" from "+request.url;
    return result;
  }
  Json body;
  try{body=response.json();}catch(...){}
  if(!body.is_object()||!body.contains("data")||!body.at("data").is_array()){
    result.message="The server answered but sent no model list.";
    return result;
  }
  std::set<std::string> seen;
  for(const auto& item:body.at("data")){
    if(!item.is_object()||!item.contains("id")||!item.at("id").is_string())continue;
    const auto id=item.at("id").get<std::string>();
    if(id.empty()||id.find_first_of("\r\n")!=std::string::npos)continue;
    if(seen.insert(id).second)result.models.push_back(id);
    if(result.models.size()>=10000)break;
  }
  result.ok=true;
  result.message="Connected to "+host+" · "+std::to_string(result.models.size())+
    (result.models.size()==1?" model":" models");
  return result;
}

SaveResult saveEndpoint(const std::filesystem::path& path,
    const std::filesystem::path& legacyPath,
    std::map<std::string,application::EndpointRecord> endpoints,
    const std::string& endpointId,application::EndpointRecord endpoint,const std::string& model){
  if(std::find(endpoint.models.begin(),endpoint.models.end(),model)==endpoint.models.end())
    endpoint.models.insert(endpoint.models.begin(),model);
  application_config::validateEndpoint(endpointId,endpoint);
  endpoints.insert_or_assign(endpointId,endpoint);
  SaveResult result;
  result.endpointId=endpointId;
  result.profile=application_config::profileId(endpointId,model);
  application_config::saveEndpoints(path,endpoints,result.profile,Json::object(),true,legacyPath);
  result.endpoint=std::move(endpoint);
  result.endpoints=std::move(endpoints);
  return result;
}

std::string addedNotice(const std::string& endpointId,const std::string& model){
  return "Added "+endpointId+" · "+model+" · /model to switch";
}
} // namespace batbox::providers

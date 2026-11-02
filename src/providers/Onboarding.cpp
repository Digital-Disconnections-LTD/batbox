#include "providers/Onboarding.hpp"
#include "application_config/EndpointStore.hpp"
#include "transport/Errors.hpp"
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
std::string firstLine(std::string text){
  if(const auto end=text.find_first_of("\r\n");end!=std::string::npos)text.resize(end);
  if(text.size()>200)text=text.substr(0,199)+"…";
  return text;
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
  endpoint.region=cloudRegion(provider.kind,endpoint.baseUrl);
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

std::string cloudRegion(const std::string& kind,const std::string& baseUrl){
  const auto host=origin(baseUrl);
  const auto valid=[](const std::string& region){
    return !region.empty()&&region.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-")==
      std::string::npos;
  };
  if(kind==transport::kProviderBedrock){
    const std::string suffix=".amazonaws.com";
    for(const char* name:{"bedrock-runtime.","bedrock-runtime-fips."}){
      const std::string prefix=name;
      if(host.starts_with(prefix)&&host.ends_with(suffix)&&
          host.size()>prefix.size()+suffix.size()){
        const auto region=host.substr(prefix.size(),host.size()-prefix.size()-suffix.size());
        if(valid(region))return region;
      }
    }
    return {};
  }
  if(kind==transport::kProviderVertex){
    if(host=="aiplatform.googleapis.com")return "global";
    const std::string suffix="-aiplatform.googleapis.com";
    if(host.ends_with(suffix)&&host.size()>suffix.size()){
      const auto region=host.substr(0,host.size()-suffix.size());
      if(valid(region))return region;
    }
  }
  return {};
}

ConnectionResult testConnection(const application::EndpointRecord& endpoint,
    ListModels list,std::chrono::milliseconds timeout){
  ConnectionResult result;
  if(!list)list=[timeout](const application::EndpointRecord& target){
    return listEndpointModels(target,timeout);};
  const bool bedrock=endpoint.provider==transport::kProviderBedrock;
  const bool vertex=endpoint.provider==transport::kProviderVertex;
  std::string url,host=bedrock?"Amazon Bedrock":vertex?"Vertex AI":"";
  if(host.empty()){
    try{url=application_config::modelsUrl(endpoint);}
    catch(const std::exception& error){result.message=error.what();return result;}
    host=origin(url);
  }
  Json body;
  try{body=list(endpoint);}
  catch(const transport::APIConnectionError&){
    result.message="Could not connect to "+host+(!url.empty()&&loopback(url)?
      ". Is the server running?":". Check the base URL and your network.");
    return result;
  }catch(const transport::APIError& error){
    result.status=static_cast<int>(error.status.value_or(0));
    result.reached=result.status!=0;
    const auto status=std::to_string(result.status);
    auto detail=firstLine(error.what());
    if(detail.starts_with(status+" "))detail.erase(0,status.size()+1);
    if(result.status==401||result.status==403)
      result.message=url.empty()?host+" did not accept your credentials (HTTP "+status+"): "+
        detail:"The server rejected the key (HTTP "+status+
        "). Check the key or its environment variable.";
    else if(result.status==404&&!url.empty())
      result.message="No model list at "+url+" (HTTP 404). The server may still "
        "answer requests; pick a model from the catalog.";
    else if(result.status>=200&&result.status<300)
      result.message="The server answered but sent no model list.";
    else if(result.status!=0)
      result.message="HTTP "+status+" from "+(url.empty()?host:url);
    else result.message=detail;
    return result;
  }catch(const std::exception& error){
    result.message=firstLine(error.what());
    return result;
  }
  result.reached=true;result.status=200;
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
  const auto count=std::to_string(result.models.size())+
    (result.models.size()==1?" model":" models");
  result.message=vertex?"Vertex AI has no model list call; Batbox checks your Google "
    "credentials on the first request · "+count:"Connected to "+host+" · "+count;
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

#include "application_config/Discovery.hpp"
#include "application_config/EndpointStore.hpp"
#include <cstdlib>
#include <set>

namespace batbox::application_config {
DiscoveryResult discoverModels(const application::EndpointRecord& endpoint,
    mcp::http::Fetch fetch){
  mcp::http::Request request;
  request.url=modelsUrl(endpoint);
  request.method="GET";
  request.followRedirects=false;
  request.timeout=std::chrono::seconds(12);
  request.connectTimeout=std::chrono::seconds(5);
  request.headers=endpoint.headers;
  request.headers["accept"]="application/json";
  std::string token=endpoint.authToken;
  if(token.empty()&&!endpoint.apiKeyEnv.empty()){
    const auto* value=std::getenv(endpoint.apiKeyEnv.c_str());
    if(value)token=value;
  }
  if(endpoint.authHeader=="authorization"&&!token.empty())
    request.headers["authorization"]="Bearer "+token;
  else if(endpoint.authHeader=="x-api-key"&&!token.empty())
    request.headers["x-api-key"]=token;
  try{
    const auto response=fetch(request);
    if(!response.ok())return {{},"Model discovery returned HTTP "+
      std::to_string(response.status),false};
    const auto data=response.json();
    if(!data.is_object()||!data.contains("data")||!data.at("data").is_array())
      return {{},"The models response has no data array",false};
    DiscoveryResult result;result.success=true;
    std::set<std::string> seen;
    for(const auto& item:data.at("data")){
      if(!item.is_object()||!item.contains("id")||!item.at("id").is_string())continue;
      const auto id=item.at("id").get<std::string>();
      if(id.empty()||id.find_first_of("\r\n")!=std::string::npos)continue;
      if(seen.insert(id).second)result.models.push_back(id);
      if(result.models.size()==10000)break;
    }
    return result;
  }catch(...){return {{},"Could not reach or parse this endpoint's model list",false};}
}
}

#include "application_config/Discovery.hpp"
#include "application_config/EndpointStore.hpp"
#include "providers/cloud/CloudClients.hpp"
#include <cstdlib>
#include <set>

namespace batbox::application_config {
namespace {
DiscoveryResult modelIds(const Json& data){
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
}
// Bedrock lists through its signed control plane; Vertex returns the endpoint's or the built-in list.
DiscoveryResult discoverCloudModels(const application::EndpointRecord& endpoint,const std::string& token){
  transport::ClientConfig network;
  network.baseUrl=endpoint.baseUrl;
  network.headers=endpoint.headers;
  network.connectTimeout=std::chrono::seconds(5);
  try{
    auto client=providers::cloud::makeCloudClient(
      {endpoint.provider,endpoint.region,endpoint.project,endpoint.models,
       endpoint.authHeader=="none"?std::string{}:token},std::move(network));
    return modelIds(client->listModels());
  }catch(const std::exception& error){
    return {{},std::string("Model discovery failed: ")+error.what(),false};
  }
}
}
DiscoveryResult discoverModels(const application::EndpointRecord& endpoint,
    mcp::http::Fetch fetch){
  std::string token=endpoint.authToken;
  if(token.empty()&&!endpoint.apiKeyEnv.empty()){
    const auto* value=std::getenv(endpoint.apiKeyEnv.c_str());
    if(value)token=value;
  }
  if(providers::cloud::isCloudKind(endpoint.provider))return discoverCloudModels(endpoint,token);
  mcp::http::Request request;
  request.url=modelsUrl(endpoint);
  request.method="GET";
  request.followRedirects=false;
  request.timeout=std::chrono::seconds(12);
  request.connectTimeout=std::chrono::seconds(5);
  request.headers=endpoint.headers;
  request.headers["accept"]="application/json";
  if(endpoint.authHeader=="authorization"&&!token.empty())
    request.headers["authorization"]="Bearer "+token;
  else if(endpoint.authHeader=="x-api-key"&&!token.empty())
    request.headers["x-api-key"]=token;
  try{
    const auto response=fetch(request);
    if(!response.ok())return {{},"Model discovery returned HTTP "+
      std::to_string(response.status),false};
    return modelIds(response.json());
  }catch(...){return {{},"Could not reach or parse this endpoint's model list",false};}
}
}

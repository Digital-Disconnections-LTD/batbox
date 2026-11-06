#include "application_config/Discovery.hpp"
#include "transport/Errors.hpp"
#include "transport/RawHttp.hpp"
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
    if(const auto context=transport::reportedContextWindow(item))result.contexts[id]=context;
    if(result.models.size()==10000)break;
  }
  return result;
}
}
DiscoveryResult discoverModels(const application::EndpointRecord& endpoint,
    providers::ListModels list){
  if(!list)list=[](const application::EndpointRecord& target){
    return providers::listEndpointModels(target,std::chrono::seconds(12));};
  try{return modelIds(list(endpoint));}
  catch(const transport::APIConnectionError&){
    return {{},"Could not reach this endpoint's model list",false};
  }catch(const transport::APIError& error){
    if(error.status&&(*error.status<200||*error.status>=300))
      return {{},"Model discovery returned HTTP "+std::to_string(*error.status)+
        (*error.status==401||*error.status==403?": the server rejected the key":""),false};
    return {{},std::string("Model discovery failed: ")+error.what(),false};
  }catch(const std::exception& error){
    return {{},std::string("Model discovery failed: ")+error.what(),false};
  }
}
}

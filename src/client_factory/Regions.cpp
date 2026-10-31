#include "client_factory/Regions.hpp"
#include <stdexcept>

namespace batbox::client_factory {
namespace {
std::optional<std::string> env(const RegionServices& services,
    const std::string& key){
  if(!services.environment)
    throw std::invalid_argument("Missing region environment binding");
  return services.environment(key);
}
}
std::string getAwsRegion(const RegionServices& services){
  if(const auto value=env(services,services.awsRegionKey);
      value&&!value->empty())return *value;
  if(const auto value=env(services,services.awsDefaultRegionKey);
      value&&!value->empty())return *value;
  return services.awsFallback;
}
std::string getDefaultVertexRegion(const RegionServices& services){
  if(const auto value=env(services,services.vertexDefaultRegionKey);
      value&&!value->empty())return *value;
  return services.vertexFallback;
}
std::string getVertexRegionForModel(
    const std::optional<std::string>& model,
    const RegionServices& services){
  if(model&&!model->empty()){
    for(const auto& [prefix,key]:services.vertexRegionOverrides){
      if(model->starts_with(prefix)){
        if(const auto value=env(services,key);
            value&&!value->empty())return *value;
        return getDefaultVertexRegion(services);
      }
    }
  }
  return getDefaultVertexRegion(services);
}
}

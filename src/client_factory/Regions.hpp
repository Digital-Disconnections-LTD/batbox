#pragma once
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace batbox::client_factory {
struct RegionServices {
  std::function<std::optional<std::string>(const std::string&)> environment;
  std::vector<std::pair<std::string,std::string>> vertexRegionOverrides;
  std::string awsRegionKey="AWS_REGION";
  std::string awsDefaultRegionKey="AWS_DEFAULT_REGION";
  std::string vertexDefaultRegionKey="CLOUD_ML_REGION";
  std::string awsFallback="us-east-1";
  std::string vertexFallback="us-east5";
};
std::string getAwsRegion(const RegionServices& services);
std::string getDefaultVertexRegion(const RegionServices& services);
std::string getVertexRegionForModel(
  const std::optional<std::string>& model,const RegionServices& services);
}

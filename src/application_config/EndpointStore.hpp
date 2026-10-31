#pragma once
#include "application/Config.hpp"
#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace batbox::application_config {
std::string profileId(const std::string& endpointId,const std::string& modelId);
std::string modelsUrl(const application::EndpointRecord& endpoint);
void validateEndpoint(const std::string& id,const application::EndpointRecord& endpoint);
void loadEndpoints(application::Config& config,const Json& source);
application::InferenceProfile inferenceProfile(const application::EndpointRecord& endpoint,
  const std::string& model);
Json endpointDocument(const std::map<std::string,application::EndpointRecord>& endpoints);
std::optional<std::map<std::string,application::EndpointRecord>> readEndpoints(
  const std::filesystem::path& path);
// Atomic replacement of the indigenous-client JSON document; unrelated keys survive.
// Permissions are private because the document can contain API credentials.
void saveEndpoints(const std::filesystem::path& path,
  const std::map<std::string,application::EndpointRecord>& endpoints,
  const std::string& selectedProfile,const Json& settingEdits=Json::object(),
  bool persistSelection=true,const std::filesystem::path& fallbackPath={});
void patchConfigSettings(const std::filesystem::path& path,const Json& edits,
  const std::filesystem::path& fallbackPath={});

}

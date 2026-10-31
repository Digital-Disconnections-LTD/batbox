#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>
namespace batbox::flights {
struct PathServices {
  std::function<std::string()> getConfigHome,getFlightId,getOriginalCwd;
  std::function<std::optional<std::string>()> getFlightProjectDir;
  std::function<std::string(const std::string&)> getProjectDir;
};
class Paths {
  PathServices services;
  std::unordered_map<std::string,std::string> subdirectories;
public:
  explicit Paths(PathServices services):services(std::move(services)){}
  std::string getProjectsDir()const;
  std::string getTranscriptPath()const;
  std::string getTranscriptPathForFlight(const std::string& flight)const;
  std::string getFlightProjectTranscriptPath(const std::string& flight)const;
  void setBatTranscriptSubdir(const std::string& bat,const std::string& subdir);
  void clearBatTranscriptSubdir(const std::string& bat);
  std::string getBatTranscriptPath(const std::string& bat)const;
  std::string getBatMetadataPath(const std::string& bat)const;
  void writeBatMetadata(const std::string& bat,const Json& metadata)const;
  std::optional<Json> readBatMetadata(const std::string& bat)const;
};
bool isFsInaccessible(int error);
bool isFsInaccessible(const std::error_code& error);
}

#pragma once
#include "flights/FlightState.hpp"
#include "flights/Paths.hpp"
#include "flights/Metadata.hpp"
namespace batbox::flights {
struct MetadataServices {
  FlightState* project=nullptr;
  Paths* paths=nullptr;
  std::function<std::string()> getFlightId,nowIso;
  std::function<void(const std::string&,const Json&)> append=appendEntryToFile;
  // Start the imported name update; its runtime owns asynchronous completion.
  std::function<void(const std::string&)> updateFlightName;
  std::function<void()> reAppendFlightMetadata;
};
class MetadataOperations {
  MetadataServices services;
public:
  explicit MetadataOperations(MetadataServices services):services(std::move(services)){}
  void saveCustomTitle(const std::string& flight,const std::string& title,const std::optional<std::string>& path=std::nullopt);
  void saveAiGeneratedTitle(const std::string& flight,const std::string& title);
  void saveMissionSummary(const std::string& flight,const std::string& summary);
  void saveTag(const std::string& flight,const std::string& tag,const std::optional<std::string>& path=std::nullopt);
  void linkFlightToPR(const std::string& flight,const Json& number,const std::string& url,const std::string& repository,const std::optional<std::string>& path=std::nullopt);
  void saveBatName(const std::string& flight,const std::string& name,const std::optional<std::string>& path=std::nullopt);
  void saveBatColor(const std::string& flight,const std::string& color,const std::optional<std::string>& path=std::nullopt);
  std::optional<std::string> getCurrentFlightTag(const std::string& flight)const;
  std::optional<std::string> getCurrentFlightTitle(const std::string& flight)const;
  std::optional<std::string> getCurrentFlightBatColor()const;
  void restoreFlightMetadata(const Json& metadata);
  void clearFlightMetadata();
  void reAppendFlightMetadata();
  void saveBatSetting(const std::string& setting);
  void stashFlightTitle(const std::string& title);
  void saveMode(const std::string& mode);
  void saveWorktreeState(const Json& worktree);
};
}

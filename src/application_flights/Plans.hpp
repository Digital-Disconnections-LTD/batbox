#pragma once
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <exception>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace batbox::application_flights {
std::optional<std::string> planSlugFromLog(const Json& log);
std::optional<std::string> recoverPlanFromMessages(const Json& log);
std::optional<Json> findFileSnapshotEntry(const Json& messages,const std::string& key);
std::string generatePlanSlug();

// Flight identity owns the association. Merely opening the same project never
// adopts another flight's plan. All calls are made on the application owner.
class PlanFiles {
public:
  using Logger=std::function<void(std::exception_ptr)>;
  PlanFiles(std::function<std::filesystem::path()> directory,
    Logger logError,std::function<std::string()> generateSlug=generatePlanSlug);
  std::string slug(const std::string& flight);
  std::filesystem::path path(const std::string& flight,
    const std::optional<std::string>& bat={});
  std::optional<std::string> content(const std::string& flight,
    const std::optional<std::string>& bat={});
  bool resume(const Json& log,const std::string& flight,bool remote=false);
  bool fork(const Json& log,const std::string& targetFlight);
  void clear(){slugs_.clear();}
private:
  std::function<std::filesystem::path()> directory_;
  Logger logError_;
  std::function<std::string()> generateSlug_;
  std::map<std::string,std::string> slugs_;
};
} // namespace batbox::application_flights

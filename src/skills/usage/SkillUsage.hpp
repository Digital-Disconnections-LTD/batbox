#pragma once
#include "runtime/JsonValue.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace batbox::skills::usage {
struct State {
  std::unordered_map<std::string,std::int64_t> lastWriteBySkill;
};
struct Services {
  std::function<std::int64_t()> nowMillis;
  std::function<Json()> getGlobalConfig;
  std::function<void(const std::function<Json(const Json&)>&)> saveGlobalConfig;
};
void recordSkillUsage(const std::string& skillName,State& state,Services& services);
double getSkillUsageScore(const std::string& skillName,Services& services);
}

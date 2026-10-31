#pragma once
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace batbox::mission_management {
// One mission list lives at <config-home>/missions/<sanitized-list-id>.  Every
// operation holds a cross-process list lock; gadget calls do disk work off owner.
class Store {
public:
  explicit Store(std::filesystem::path configHome);
  std::string create(const std::string& listId,Json mission);
  std::optional<Json> get(const std::string& listId,const std::string& missionId)const;
  std::vector<Json> list(const std::string& listId)const;
  std::optional<Json> update(const std::string& listId,const std::string& missionId,
    const Json& fields);
  bool erase(const std::string& listId,const std::string& missionId);
  bool block(const std::string& listId,const std::string& fromId,
    const std::string& toId);
private:
  std::filesystem::path directory(const std::string& listId)const;
  static std::filesystem::path missionPath(const std::filesystem::path& directory,
    const std::string& missionId);
  std::filesystem::path home_;
};
} // namespace batbox::mission_management

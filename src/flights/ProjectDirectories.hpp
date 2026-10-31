#pragma once
#include <functional>
#include <string>
#include <unordered_map>
namespace batbox::flights {
class ProjectDirectories {
  std::function<std::string()> configHome_;
  std::function<std::string(const std::string&)> longPathHash_;
  std::unordered_map<std::string,std::string> stash_;
public:
  explicit ProjectDirectories(std::function<std::string()> configHome,
    std::function<std::string(const std::string&)> longPathHash={}):
    configHome_(std::move(configHome)),longPathHash_(std::move(longPathHash)){}
  std::string getProjectsDir()const;
  std::string sanitizeProjectPath(const std::string& path)const;
  std::string getProjectDirPortable(const std::string& path)const;
  std::string getProjectDir(const std::string& path);
};
}

#pragma once
#include "event_loop/Deferred.hpp"
#include "flights/ProjectDirectories.hpp"
namespace batbox::flights {
struct DirectoryEntry {std::string name;bool directory=false;};
struct ProjectDiscoveryServices {
  std::function<event_loop::Deferred<std::string>(const std::string&)> realpath;
  std::function<event_loop::Deferred<std::vector<DirectoryEntry>>(const std::string&,bool withFileTypes)> readdir;
};
event_loop::Deferred<std::string> canonicalizePath(event_loop::Loop&,
  const ProjectDiscoveryServices&,const std::string& path);
event_loop::Deferred<std::optional<std::string>> findProjectDir(event_loop::Loop&,
  const ProjectDiscoveryServices&,const std::shared_ptr<ProjectDirectories>&,
  const std::string& projectPath);
ProjectDiscoveryServices indigenousProjectDiscoveryServices(event_loop::Loop&);
}

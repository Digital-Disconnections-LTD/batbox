#include "stash_paths/StashPaths.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <iterator>
#include <string>

int main(){
  const std::string raw(std::istreambuf_iterator<char>(std::cin),{});
  const auto input=batbox::value::parse(raw);
  std::string cwd=input.value("cwd",std::string());
  batbox::stash_paths::StashPaths paths({
    [stash=input.at("cache").get<std::string>()]{return stash;},
    [&cwd]{return cwd;},batbox::stash_paths::indigenousPosixJoin});
  batbox::Json out=batbox::Json::array();
  for(const auto& action:input.at("actions")){
    const auto op=action.at("op").get<std::string>();
    if(op=="cwd"){cwd=action.at("value").get<std::string>();out.push_back(nullptr);continue;}
    if(op=="sanitize")out.push_back(batbox::stash_paths::sanitizePath(action.at("value")));
    if(op=="project")out.push_back(batbox::stash_paths::getProjectDir(action.at("value")));
    if(op=="baseLogs")out.push_back(paths.baseLogs());
    if(op=="errors")out.push_back(paths.errors());
    if(op=="messages")out.push_back(paths.messages());
    if(op=="mcpLogs")out.push_back(paths.mcpLogs(action.at("value")));
  }
  std::cout<<batbox::value::stringify(out);
}

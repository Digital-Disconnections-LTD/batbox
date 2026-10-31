#include "instructions/project/ProjectMemory.hpp"
#include "instructions/Frontmatter.hpp"

namespace batbox::instructions::project {
FrontmatterPaths parseFrontmatterPaths(const std::string& rawContent){
  const auto parsed=instructions::parseFrontmatter(rawContent);
  const auto& rawPaths=value::property(parsed.frontmatter,"paths");
  if(!value::truthy(rawPaths))return {parsed.content,std::nullopt};
  const auto paths=instructions::splitPathInFrontmatter(rawPaths);
  std::vector<std::string> patterns;
  for(auto pattern:paths){
    if(pattern.ends_with("/**"))pattern.resize(pattern.size()-3);
    if(!pattern.empty())patterns.push_back(std::move(pattern));
  }
  if(patterns.empty())return {parsed.content,std::nullopt};
  bool allMatch=true;
  for(const auto& pattern:patterns)if(pattern!="**"){allMatch=false;break;}
  return {parsed.content,allMatch?std::nullopt:
    std::optional<std::vector<std::string>>(std::move(patterns))};
}
}

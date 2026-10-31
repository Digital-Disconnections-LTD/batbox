#include "skills/Skill.hpp"
#include "instructions/Frontmatter.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace batbox::skills {
namespace {
std::string readUtf8File(const std::string& path){
  const auto indigenousPath=runtime::pathFromUtf8(path);
  std::ifstream input(indigenousPath,std::ios::binary);
  if(!input)throw std::filesystem::filesystem_error("readFile",
    indigenousPath,std::make_error_code(std::errc::no_such_file_or_directory));
  return value::decodeUtf8(std::string(std::istreambuf_iterator<char>(input),{}));
}
}
std::vector<SkillWithPath> loadSkillsFromSkillsDir(const std::string& basePath,
  const std::string& source,const SkillServices& services){
  std::vector<SkillWithPath> result;
  std::error_code error;
  std::filesystem::directory_iterator entries(runtime::pathFromUtf8(basePath),error);
  if(error){
    if(error!=std::errc::no_such_file_or_directory&&
       error!=std::errc::permission_denied&&services.logError)
      services.logError(error.message());
    return result;
  }
  std::vector<std::filesystem::directory_entry> sorted;
  for(const auto& entry:entries)sorted.push_back(entry);
  std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){
    return runtime::pathToUtf8(a.path().filename())<
      runtime::pathToUtf8(b.path().filename());
  });
  for(const auto& entry:sorted){
    try{
      if(!entry.is_directory()&&!entry.is_symlink())continue;
      const auto skillDir=entry.path();
      const auto filePath=runtime::pathToUtf8(skillDir/"SKILL.md");
      std::string content;
      try{content=readUtf8File(filePath);}
      catch(const std::exception& readError){
        if(std::filesystem::exists(runtime::pathFromUtf8(filePath))&&services.logDebug)
          services.logDebug("[skills] failed to read "+filePath+": "+readError.what());
        continue;
      }
      const auto parsed=instructions::parseFrontmatter(content,filePath,services.logDebug);
      const auto name=runtime::pathToUtf8(entry.path().filename());
      const auto fields=parseSkillFrontmatterFields(parsed.frontmatter,
        parsed.content,name,"Skill",services);
      const auto paths=parseSkillPaths(parsed.frontmatter);
      result.push_back({createSkillCommand(fields,name,parsed.content,source,
        runtime::pathToUtf8(skillDir),"skills",paths),filePath});
    }catch(const std::exception& processingError){
      if(services.logError)services.logError(processingError.what());
    }
  }
  return result;
}
}

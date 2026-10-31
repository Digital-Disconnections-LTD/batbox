#include "gadgets/files/GrepInternal.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <stdexcept>

namespace batbox::gadgets::files {
namespace {
const std::vector<std::string> vcs={".git",".svn",".hg",".bzr",".jj",".sl"};
bool has(const Json& object,const char* key){return object.contains(key)&&!object.at(key).is_null();}
std::vector<std::string> splitGlob(const std::string& glob){
  std::vector<std::string> patterns;
  std::size_t i=0;
  while(i<glob.size()){
    while(i<glob.size()&&std::isspace(static_cast<unsigned char>(glob[i])))++i;
    if(i>=glob.size())break;
    const auto start=i;
    while(i<glob.size()&&!std::isspace(static_cast<unsigned char>(glob[i])))++i;
    const auto raw=glob.substr(start,i-start);
    if(raw.find('{')!=std::string::npos&&raw.find('}')!=std::string::npos){patterns.push_back(raw);continue;}
    std::size_t j=0;
    while(j<raw.size()){
      const auto comma=raw.find(',',j);
      const auto part=raw.substr(j,comma==std::string::npos?std::string::npos:comma-j);
      if(!part.empty())patterns.push_back(part);
      if(comma==std::string::npos)break;
      j=comma+1;
    }
  }
  return patterns;
}
void require(bool bound,const char* name){
  if(!bound)throw std::invalid_argument(std::string("Missing Grep binding: ")+name);
}
} // namespace

GadgetResult callGrep(const Json& input,const GadgetCallContext& context,const GrepServices& services){
  const auto cwd=services.paths.getCwd ? services.paths.getCwd() : std::filesystem::current_path().string();
  const auto absolutePath=has(input,"path")&&value::truthy(input.at("path"))
    ? filesystem::expandPath(input.at("path").get<std::string>(),services.paths) : cwd;
  const auto mode=input.value("output_mode",std::string("files_with_matches"));
  const auto pattern=input.at("pattern").get<std::string>();
  const auto offset=input.value("offset",0.0);
  const auto headLimit=has(input,"head_limit")?std::optional<double>(input.at("head_limit").get<double>()):std::nullopt;
  Json permission=context.data.is_object()&&context.data.contains("toolPermissionContext")
    ? context.data.at("toolPermissionContext") : Json::object();
  std::vector<std::string> args={"--hidden"};
  for(const auto& dir:vcs){args.push_back("--glob");args.push_back("!"+dir);}
  args.push_back("--max-columns");args.push_back("500");
  if(input.value("multiline",false)){args.push_back("-U");args.push_back("--multiline-dotall");}
  if(input.value("-i",false))args.push_back("-i");
  if(mode=="files_with_matches")args.push_back("-l");
  else if(mode=="count")args.push_back("-c");
  if(mode=="content"&&input.value("-n",true))args.push_back("-n");
  if(mode=="content"){
    if(has(input,"context")){args.push_back("-C");args.push_back(value::numberToString(input.at("context").get<double>()));}
    else if(has(input,"-C")){args.push_back("-C");args.push_back(value::numberToString(input.at("-C").get<double>()));}
    else{
      if(has(input,"-B")){args.push_back("-B");args.push_back(value::numberToString(input.at("-B").get<double>()));}
      if(has(input,"-A")){args.push_back("-A");args.push_back(value::numberToString(input.at("-A").get<double>()));}
    }
  }
  if(pattern.starts_with('-')){args.push_back("-e");args.push_back(pattern);}
  else args.push_back(pattern);
  if(has(input,"type")&&value::truthy(input.at("type"))){args.push_back("--type");args.push_back(input.at("type").get<std::string>());}
  if(has(input,"glob")&&value::truthy(input.at("glob")))
    for(const auto& item:splitGlob(input.at("glob").get<std::string>())){
      args.push_back("--glob");args.push_back(item);
    }
  require(static_cast<bool>(services.getFileReadIgnorePatterns),"getFileReadIgnorePatterns");
  require(static_cast<bool>(services.normalizePatternsToPath),"normalizePatternsToPath");
  const auto ignored=services.normalizePatternsToPath(
    services.getFileReadIgnorePatterns(permission),cwd);
  for(const auto& item:ignored){
    args.push_back("--glob");args.push_back(item.starts_with('/')?"!"+item:"!**/"+item);
  }
  require(static_cast<bool>(services.getGlobExclusionsForPluginStash),"getGlobExclusionsForPluginCache");
  for(const auto& exclusion:services.getGlobExclusionsForPluginStash(absolutePath)){
    args.push_back("--glob");args.push_back(exclusion);
  }
  require(static_cast<bool>(services.ripGrep),"ripGrep");
  const auto results=services.ripGrep(args,absolutePath,context.stopToken);
  if(mode=="content")return grepContentResult(results,headLimit,offset,cwd);
  if(mode=="count")return grepCountResult(results,headLimit,offset,cwd);
  return grepFilesResult(results,headLimit,offset,cwd,services);
}
} // namespace batbox::gadgets::files

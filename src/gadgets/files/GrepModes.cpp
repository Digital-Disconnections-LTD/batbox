#include "gadgets/files/GrepInternal.hpp"
#include "gadgets/files/Glob.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>

namespace batbox::gadgets::files {
namespace {
std::string join(const std::vector<std::string>& values){
  std::string result;
  for(const auto& value:values){if(!result.empty())result+='\n';result+=value;}
  return result;
}
std::string relativizeFirstColon(const std::string& line,const std::string& cwd){
  const auto colon=line.find(':');
  if(colon==std::string::npos||colon==0)return line;
  return toRelativePath(line.substr(0,colon),cwd)+line.substr(colon);
}
std::string relativizeLastColon(const std::string& line,const std::string& cwd){
  const auto colon=line.rfind(':');
  if(colon==std::string::npos||colon==0)return line;
  return toRelativePath(line.substr(0,colon),cwd)+line.substr(colon);
}
void addPagination(Json& output,const HeadLimitResult& limited,double offset){
  if(limited.appliedLimit)output["appliedLimit"]=*limited.appliedLimit;
  if(offset>0)output["appliedOffset"]=offset;
}
} // namespace

GadgetResult grepContentResult(const std::vector<std::string>& results,
  std::optional<double> headLimit,double offset,const std::string& cwd){
  const auto limited=applyHeadLimit(results,headLimit,offset);
  std::vector<std::string> lines;
  lines.reserve(limited.items.size());
  for(const auto& line:limited.items)lines.push_back(relativizeFirstColon(line,cwd));
  Json output={{"mode","content"},{"numFiles",0},{"filenames",Json::array()},
    {"content",join(lines)},{"numLines",lines.size()}};
  addPagination(output,limited,offset);
  GadgetResult result;result.data=output;return result;
}

GadgetResult grepCountResult(const std::vector<std::string>& results,
  std::optional<double> headLimit,double offset,const std::string& cwd){
  const auto limited=applyHeadLimit(results,headLimit,offset);
  std::vector<std::string> lines;
  lines.reserve(limited.items.size());
  for(const auto& line:limited.items)lines.push_back(relativizeLastColon(line,cwd));
  double totalMatches=0,fileCount=0;
  for(const auto& line:lines){
    const auto colon=line.rfind(':');
    if(colon==std::string::npos||colon==0)continue;
    const auto count=value::parseInt10(line.substr(colon+1));
    if(!std::isnan(count)){totalMatches+=count;fileCount+=1;}
  }
  Json output={{"mode","count"},{"numFiles",fileCount},{"filenames",Json::array()},
    {"content",join(lines)},{"numMatches",totalMatches}};
  addPagination(output,limited,offset);
  GadgetResult result;result.data=output;return result;
}
} // namespace batbox::gadgets::files

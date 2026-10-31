#include "instructions/MemoryScan.hpp"
#include "instructions/Frontmatter.hpp"
#include "instructions/MemoryTypes.hpp"
#include "filesystem/Range.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/IndigenousTime.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>

namespace batbox::instructions {
namespace {
constexpr std::size_t maxMemoryFiles=200;
constexpr std::size_t frontmatterMaxLines=30;
std::string isoDate(double mtimeMs){
  if(!std::isfinite(mtimeMs))throw std::runtime_error("Invalid time value");
  const auto total=static_cast<std::int64_t>(std::floor(mtimeMs));
  std::int64_t seconds=total/1000;
  int millis=static_cast<int>(total%1000);
  if(millis<0){--seconds;millis+=1000;}
  const std::time_t time=static_cast<std::time_t>(seconds);
  std::tm utc{};
  if(!runtime::gmtimeUtc(time,utc))throw std::runtime_error("Invalid time value");
  std::ostringstream output;
  output<<std::put_time(&utc,"%Y-%m-%dT%H:%M:%S")<<'.'
    <<std::setfill('0')<<std::setw(3)<<millis<<'Z';
  return output.str();
}
std::string jsText(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_array()){
    std::string result;bool first=true;
    for(const auto& item:value){if(!first)result+=',';first=false;
      if(!item.is_null())result+=jsText(item);}
    return result;
  }
  return value.is_null()?"null":"[object Object]";
}
}
std::vector<MemoryHeader> scanMemoryFiles(const std::string& memoryDir,
  std::stop_token signal){
  try{
    std::vector<std::string> relativePaths;
    const auto base=runtime::pathFromUtf8(memoryDir);
    std::vector<std::filesystem::path> pending={base};
    while(!pending.empty()){
      const auto dir=pending.back();pending.pop_back();
      std::vector<std::filesystem::directory_entry> entries;
      for(const auto& entry:std::filesystem::directory_iterator(dir))
        entries.push_back(entry);
      std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){
        return runtime::pathToUtf8(a.path().filename())<
          runtime::pathToUtf8(b.path().filename());
      });
      for(const auto& entry:entries){
        const auto relative=entry.path().lexically_relative(base);
        if(relative.extension()==".md"&&relative.filename()!="MEMORY.md")
          relativePaths.push_back(runtime::pathToUtf8(relative));
        if(entry.is_directory()&&!entry.is_symlink())pending.push_back(entry.path());
      }
    }
    std::vector<MemoryHeader> headers;
    for(const auto& relative:relativePaths){
      try{
        const auto filePath=runtime::pathToUtf8(base/runtime::pathFromUtf8(relative));
        const auto range=filesystem::readFileInRange(filePath,0,frontmatterMaxLines,
          std::nullopt,signal);
        const auto parsed=parseFrontmatter(value::decodeUtf8(range.content),filePath);
        const auto description=value::property(parsed.frontmatter,"description");
        MemoryHeader header{relative,filePath,range.mtimeMs,nullptr,std::nullopt};
        if(value::truthy(description))header.description=description;
        header.type=parseMemoryType(value::property(parsed.frontmatter,"type"));
        headers.push_back(std::move(header));
      }catch(const std::exception&){}
    }
    std::stable_sort(headers.begin(),headers.end(),[](const auto& a,const auto& b){
      return a.mtimeMs>b.mtimeMs;
    });
    if(headers.size()>maxMemoryFiles)headers.resize(maxMemoryFiles);
    return headers;
  }catch(const std::exception&){return {};}
}
std::string formatMemoryManifest(const std::vector<MemoryHeader>& memories){
  std::string result;
  for(const auto& item:memories){
    if(!result.empty())result+='\n';
    result+="- ";
    if(item.type)result+='['+*item.type+"] ";
    result+=item.filename+" ("+isoDate(item.mtimeMs)+')';
    if(value::truthy(item.description))result+=": "+jsText(item.description);
  }
  return result;
}
double memoryAgeDays(double mtimeMs,double nowMs){
  if(std::isnan(mtimeMs)||std::isnan(nowMs))
    return std::numeric_limits<double>::quiet_NaN();
  return std::max(0.0,std::floor((nowMs-mtimeMs)/86400000.0));
}
std::string memoryAge(double mtimeMs,double nowMs){
  const auto days=memoryAgeDays(mtimeMs,nowMs);
  if(days==0)return "today";
  if(days==1)return "yesterday";
  return value::numberToString(days)+" days ago";
}
std::string memoryFreshnessText(double mtimeMs,double nowMs){
  const auto days=memoryAgeDays(mtimeMs,nowMs);
  if(days<=1)return {};
  return "This memory is "+value::numberToString(days)+" days old. "
    "Memories are point-in-time observations, not live state — "
    "claims about code behavior or file:line citations may be outdated. "
    "Verify against current code before asserting as fact.";
}
std::string memoryFreshnessNote(double mtimeMs,double nowMs){
  const auto content=memoryFreshnessText(mtimeMs,nowMs);
  return content.empty()?std::string{}:"<system-reminder>"+content+"</system-reminder>\n";
}
}

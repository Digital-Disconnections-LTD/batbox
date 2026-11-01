#include "flights/SavedBats.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <optional>
#include <system_error>

namespace batbox::flights {
namespace {
namespace fs=std::filesystem;
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string text(const Json& value){return value.is_string()?value.get<std::string>():"";}
constexpr std::size_t headRows=8;
constexpr std::streamoff tailBytes=64*1024;
constexpr std::size_t countChunk=256*1024;
std::optional<Json> parseRow(const std::string& line){
  if(line.empty()||line.front()!='{')return std::nullopt;
  try{return value::parse(line);}catch(const std::exception&){return std::nullopt;}
}
std::string poetryOf(const Json& row){
  const auto& content=field(field(row,"message"),"content");
  if(content.is_string())return content.get<std::string>();
  std::string joined;
  if(content.is_array())for(const auto& block:content)
    if(field(block,"type")=="text"){
      if(!joined.empty())joined+='\n';
      joined+=text(field(block,"text"));
    }
  return joined;
}
std::size_t countOccurrences(std::ifstream& input,const std::string& needle){
  std::size_t count=0;
  std::string carry,buffer(countChunk,'\0');
  input.clear();input.seekg(0);
  while(input){
    input.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));
    const auto got=static_cast<std::size_t>(input.gcount());
    if(!got)break;
    std::string window=carry+buffer.substr(0,got);
    for(auto at=window.find(needle);at!=std::string::npos;at=window.find(needle,at+needle.size()))
      ++count;
    carry=window.size()>=needle.size()?window.substr(window.size()-needle.size()+1):window;
  }
  return count;
}
std::optional<SavedBat> readSavedBat(const fs::path& transcript,const std::string& subdirectory){
  const auto name=runtime::pathToUtf8(transcript.filename());
  if(name.size()<=12||!name.starts_with("agent-")||!name.ends_with(".jsonl"))return std::nullopt;
  SavedBat bat;
  bat.id=name.substr(6,name.size()-12);bat.subdirectory=subdirectory;
  std::ifstream input(transcript,std::ios::binary);
  if(!input)return std::nullopt;
  std::string line;bool sawRow=false;
  for(std::size_t row=0;row<headRows&&std::getline(input,line);++row){
    const auto parsed=parseRow(line);if(!parsed)continue;
    sawRow=true;
    if(!bat.startTime)bat.startTime=savedBatTimestamp(text(field(*parsed,"timestamp")));
    if(bat.poetry.empty()&&field(*parsed,"type")=="user")bat.poetry=poetryOf(*parsed);
  }
  input.clear();input.seekg(0,std::ios::end);
  const auto size=static_cast<std::streamoff>(input.tellg());
  const auto start=std::max<std::streamoff>(0,size-tailBytes);
  input.seekg(start);
  std::string tail(static_cast<std::size_t>(size-start),'\0');
  input.read(tail.data(),static_cast<std::streamsize>(tail.size()));
  tail.resize(static_cast<std::size_t>(input.gcount()));
  std::size_t end=tail.size();
  while(end>0){
    const auto newline=tail.rfind('\n',end-1);
    const auto begin=newline==std::string::npos?0:newline+1;
    // The first row of a mid-file tail window is usually cut; parseRow rejects it.
    if(begin>0||start==0){
      if(const auto parsed=parseRow(tail.substr(begin,end-begin))){
        sawRow=true;
        if(!bat.endTime)bat.endTime=savedBatTimestamp(text(field(*parsed,"timestamp")));
        if(field(*parsed,"type")=="assistant"){bat.lastAssistant=*parsed;break;}
      }
    }
    if(newline==std::string::npos)break;
    end=newline;
  }
  if(!sawRow)return std::nullopt;
  bat.gadgetUses=countOccurrences(input,"\"type\":\"tool_use\"");
  std::error_code error;
  if(!bat.endTime){
    const auto modified=fs::last_write_time(transcript,error);
    if(!error){
      const auto system=std::chrono::system_clock::now()+
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
          modified-fs::file_time_type::clock::now());
      bat.endTime=std::chrono::duration_cast<std::chrono::milliseconds>(
        system.time_since_epoch()).count();
    }
  }
  if(!bat.startTime)bat.startTime=bat.endTime;
  auto metaPath=transcript;metaPath.replace_extension(".meta.json");
  std::ifstream meta(metaPath,std::ios::binary);
  if(meta){
    const std::string raw((std::istreambuf_iterator<char>(meta)),{});
    try{
      const auto parsed=value::parse(raw);
      bat.description=text(field(parsed,"description"));
      bat.agentType=text(field(parsed,"agentType"));
    }catch(const std::exception&){}
  }
  return bat;
}
}
std::int64_t savedBatTimestamp(const std::string& value){
  int y=0,m=0,d=0,h=0,n=0,s=0;
  if(std::sscanf(value.c_str(),"%d-%d-%dT%d:%d:%d",&y,&m,&d,&h,&n,&s)!=6)return 0;
  using namespace std::chrono;
  const year_month_day date{year(y),month(static_cast<unsigned>(m)),day(static_cast<unsigned>(d))};
  if(!date.ok())return 0;
  auto result=static_cast<std::int64_t>(duration_cast<milliseconds>(
    sys_days(date).time_since_epoch()).count());
  result+=static_cast<std::int64_t>(h*3600+n*60+s)*1000;
  if(const auto dot=value.find('.',19);dot!=std::string::npos){
    int multiplier=100;
    for(auto i=dot+1;i<value.size()&&multiplier>0;++i){
      if(value[i]<'0'||value[i]>'9')break;
      result+=(value[i]-'0')*multiplier;multiplier/=10;
    }
  }
  return result;
}
std::vector<SavedBat> scanSavedBats(const fs::path& directory,std::size_t limit){
  std::vector<SavedBat> found;
  std::error_code error;
  if(!fs::is_directory(directory,error))return found;
  // Rank sidechains by modification time first so only the newest are read.
  struct Candidate {fs::path path;std::string subdirectory;fs::file_time_type modified;};
  std::vector<Candidate> candidates;
  const auto visit=[&](const fs::path& folder,const std::string& subdirectory){
    std::error_code walk;
    for(fs::directory_iterator it(folder,walk),end;!walk&&it!=end;it.increment(walk)){
      std::error_code kind;
      if(!it->is_regular_file(kind))continue;
      const auto name=runtime::pathToUtf8(it->path().filename());
      if(name.size()<=12||!name.starts_with("agent-")||!name.ends_with(".jsonl"))continue;
      std::error_code stamp;
      const auto modified=it->last_write_time(stamp);
      candidates.push_back({it->path(),subdirectory,
        stamp?fs::file_time_type::min():modified});
    }
  };
  visit(directory,"");
  for(fs::directory_iterator it(directory,error),end;!error&&it!=end;it.increment(error)){
    std::error_code kind;
    if(it->is_directory(kind))visit(it->path(),runtime::pathToUtf8(it->path().filename()));
  }
  std::stable_sort(candidates.begin(),candidates.end(),
    [](const Candidate& a,const Candidate& b){return a.modified>b.modified;});
  for(const auto& candidate:candidates){
    if(found.size()>=limit)break;
    if(auto bat=readSavedBat(candidate.path,candidate.subdirectory))found.push_back(std::move(*bat));
  }
  std::stable_sort(found.begin(),found.end(),[](const SavedBat& a,const SavedBat& b){
    return a.endTime>b.endTime;
  });
  if(found.size()>limit)found.resize(limit);
  return found;
}
}

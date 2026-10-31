#include "gadgets/mission_management/Store.hpp"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace batbox::mission_management {
namespace {
class ListLock {
public:
  explicit ListLock(const std::filesystem::path& directory){
    std::filesystem::create_directories(directory);
    const auto path=directory/".lock";
#ifdef _WIN32
    handle_=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,
      FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle_==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open task list lock");
    if(!LockFileEx(handle_,LOCKFILE_EXCLUSIVE_LOCK,0,1,0,&overlapped_)){
      CloseHandle(handle_);handle_=INVALID_HANDLE_VALUE;
      throw std::runtime_error("Cannot lock task list");
    }
#else
    fd_=open(path.c_str(),O_CREAT|O_RDWR,0600);
    if(fd_<0)throw std::runtime_error("Cannot open task list lock");
    if(flock(fd_,LOCK_EX)<0){close(fd_);fd_=-1;
      throw std::runtime_error("Cannot lock task list");}
#endif
  }
  ListLock(const ListLock&)=delete;
  ListLock& operator=(const ListLock&)=delete;
  ~ListLock(){
#ifdef _WIN32
    if(handle_!=INVALID_HANDLE_VALUE){UnlockFileEx(handle_,0,1,0,&overlapped_);
      CloseHandle(handle_);}
#else
    if(fd_>=0){flock(fd_,LOCK_UN);close(fd_);}
#endif
  }
private:
#ifdef _WIN32
  HANDLE handle_=INVALID_HANDLE_VALUE;
  OVERLAPPED overlapped_{};
#else
  int fd_=-1;
#endif
};
std::string sanitize(const std::string& source){
  std::string result;result.reserve(source.size());
  for(unsigned char c:source)
    result+=((std::isalnum(c)&&c<128)||c=='-'||c=='_')?
      static_cast<char>(c):'-';
  return result;
}
bool valid(const Json& mission){
  if(!mission.is_object())return false;
  for(const char* key:{"id","subject","description","status"})
    if(!mission.contains(key)||!mission.at(key).is_string())return false;
  const auto status=mission.at("status").get<std::string>();
  if(status!="pending"&&status!="in_progress"&&status!="completed")return false;
  for(const char* key:{"blocks","blockedBy"}){
    if(!mission.contains(key)||!mission.at(key).is_array())return false;
    for(const auto& item:mission.at(key))if(!item.is_string())return false;
  }
  for(const char* key:{"activeForm","owner"})
    if(mission.contains(key)&&!mission.at(key).is_string())return false;
  return !mission.contains("metadata")||mission.at("metadata").is_object();
}
std::optional<Json> readMission(const std::filesystem::path& path){
  std::ifstream input(path,std::ios::binary);
  if(!input)return std::nullopt;
  try{auto mission=Json::parse(input);return valid(mission)?std::optional<Json>(std::move(mission)):
    std::nullopt;}catch(...){return std::nullopt;}
}
void writeMission(const std::filesystem::path& path,const Json& mission){
  static std::atomic<unsigned long long> sequence{0};
#ifdef _WIN32
  const auto process=GetCurrentProcessId();
#else
  const auto process=getpid();
#endif
  auto temporary=path;
  temporary+=".tmp."+std::to_string(process)+"."+
    std::to_string(sequence.fetch_add(1,std::memory_order_relaxed));
  try{
    {std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
      if(!output)throw std::runtime_error("Cannot write task file");
      output<<mission.dump(2);output.flush();
      if(!output)throw std::runtime_error("Cannot flush task file");}
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),path.c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
      throw std::runtime_error("Cannot replace task file");
#else
    std::filesystem::rename(temporary,path);
#endif
  }catch(...){std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}
}
std::vector<Json> listUnlocked(const std::filesystem::path& directory){
  std::vector<Json> missions;
  std::error_code error;
  for(std::filesystem::directory_iterator it(directory,error),end;!error&&it!=end;
      it.increment(error)){
    const auto path=it->path();
    if(path.extension()!=".json")continue;
    if(const auto mission=readMission(path))missions.push_back(*mission);
  }
  std::sort(missions.begin(),missions.end(),[](const Json& a,const Json& b){
    const auto number=[](const Json& item){
      try{return std::stoull(item.at("id").get<std::string>());}catch(...){return 0ULL;}};
    return number(a)<number(b);
  });
  return missions;
}
unsigned long long mark(const std::filesystem::path& directory){
  std::ifstream input(directory/".highwatermark",std::ios::binary);
  unsigned long long value=0;if(input)input>>value;return value;
}
unsigned long long number(const std::string& id){
  try{return std::stoull(id);}catch(...){return 0;}
}
void appendUnique(Json& array,const std::string& id){
  for(const auto& item:array)if(item==id)return;
  array.push_back(id);
}
}
Store::Store(std::filesystem::path configHome):home_(std::move(configHome)){
  if(home_.empty())throw std::invalid_argument("Task config home is required");
}
std::filesystem::path Store::directory(const std::string& listId)const{
  if(listId.empty())throw std::invalid_argument("Task list ID is required");
  return home_/"tasks"/sanitize(listId);
}
std::filesystem::path Store::missionPath(const std::filesystem::path& directory,
    const std::string& missionId){
  return directory/(sanitize(missionId)+".json");
}
std::string Store::create(const std::string& listId,Json mission){
  const auto dir=directory(listId);ListLock lock(dir);
  unsigned long long highest=mark(dir);
  for(const auto& current:listUnlocked(dir))
    highest=std::max(highest,number(current.at("id").get<std::string>()));
  const auto id=std::to_string(highest+1);mission["id"]=id;
  if(!valid(mission))throw std::invalid_argument("Invalid task data");
  writeMission(missionPath(dir,id),mission);return id;
}
std::optional<Json> Store::get(const std::string& listId,const std::string& missionId)const{
  const auto dir=directory(listId);ListLock lock(dir);
  return readMission(missionPath(dir,missionId));
}
std::vector<Json> Store::list(const std::string& listId)const{
  const auto dir=directory(listId);ListLock lock(dir);return listUnlocked(dir);
}
std::optional<Json> Store::update(const std::string& listId,const std::string& missionId,
    const Json& fields){
  const auto dir=directory(listId);ListLock lock(dir);
  const auto path=missionPath(dir,missionId);auto mission=readMission(path);
  if(!mission)return std::nullopt;
  for(auto it=fields.begin();it!=fields.end();++it)
    if(it.key()!="id")(*mission)[it.key()]=it.value();
  if(!valid(*mission))throw std::invalid_argument("Invalid task update");
  writeMission(path,*mission);return mission;
}
bool Store::erase(const std::string& listId,const std::string& missionId){
  const auto dir=directory(listId);ListLock lock(dir);
  const auto current=mark(dir);const auto id=number(missionId);
  if(id>current){std::ofstream output(dir/".highwatermark",std::ios::trunc);
    if(!output)throw std::runtime_error("Cannot write task high water mark");
    output<<id;}
  std::error_code error;
  if(!std::filesystem::remove(missionPath(dir,missionId),error)||error)return false;
  for(auto mission:listUnlocked(dir)){
    bool changed=false;
    for(const char* key:{"blocks","blockedBy"}){
      Json filtered=Json::array();
      for(const auto& value:mission.at(key))if(value!=missionId)filtered.push_back(value);
      if(filtered!=mission.at(key)){mission[key]=std::move(filtered);changed=true;}
    }
    if(changed)writeMission(missionPath(dir,mission.at("id").get<std::string>()),mission);
  }
  return true;
}
bool Store::block(const std::string& listId,const std::string& fromId,
    const std::string& toId){
  const auto dir=directory(listId);ListLock lock(dir);
  auto from=readMission(missionPath(dir,fromId)),to=readMission(missionPath(dir,toId));
  if(!from||!to)return false;
  if(fromId==toId){
    appendUnique((*from)["blocks"],toId);
    appendUnique((*from)["blockedBy"],fromId);
    writeMission(missionPath(dir,fromId),*from);return true;
  }
  appendUnique((*from)["blocks"],toId);
  appendUnique((*to)["blockedBy"],fromId);
  writeMission(missionPath(dir,fromId),*from);
  writeMission(missionPath(dir,toId),*to);
  return true;
}
} // namespace batbox::mission_management

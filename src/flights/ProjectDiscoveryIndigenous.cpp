#include "runtime/AsyncLaunch.hpp"
#include "flights/IndigenousPath.hpp"
#include "flights/ProjectDiscovery.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Encoding.hpp"
#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <thread>
namespace batbox::flights {
namespace {
template<class Operation>auto dispatch(event_loop::Loop& loop,Operation operation){
  using T=decltype(operation());event_loop::Deferred<T> result(loop);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());const auto dispatcher=loop.dispatcher();
  try{::batbox::runtime::contextThread([operation=std::move(operation),result,keep,dispatcher]{
    std::shared_ptr<T> value;std::exception_ptr error;
    try{value=std::make_shared<T>(operation());}catch(...){error=std::current_exception();}
    try{dispatcher.postMission([value,error,result,keep]{
      if(error)result.reject(error);else result.resolve(std::move(*value));keep->release();
    });}catch(...){keep->release();}
  }).detach();}catch(...){keep->release();result.reject(std::current_exception());}
  return result;
}
void checkPath(const std::string& path){
  if(path.find('\0')!=std::string::npos)throw std::invalid_argument("Filesystem path contains null bytes");
}
}
ProjectDiscoveryServices indigenousProjectDiscoveryServices(event_loop::Loop& loop){
  ProjectDiscoveryServices services;
  services.realpath=[&loop](const std::string& path){return dispatch(loop,[path]{
    checkPath(path);
    const auto bytes=std::filesystem::canonical(
      indigenousPath(value::encodeUtf8(path))).u8string();
    return std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size());
  });};
  services.readdir=[&loop](const std::string& path,bool withFileTypes){return dispatch(loop,[path,withFileTypes]{
    checkPath(path);std::vector<DirectoryEntry> entries;
    for(const auto& entry:std::filesystem::directory_iterator(
        indigenousPath(value::encodeUtf8(path)))){
      const auto bytes=entry.path().filename().u8string();
      entries.push_back({std::string(reinterpret_cast<const char*>(bytes.data()),
        bytes.size()),withFileTypes&&entry.symlink_status().type()==
        std::filesystem::file_type::directory});
    }
    // libuv scandir sorts directory names before Node decodes them as UTF-8.
    std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.name<b.name;});
    for(auto& entry:entries)entry.name=value::decodeUtf8(entry.name);
    return entries;
  });};
  return services;
}
}

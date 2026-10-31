#include "runtime/AsyncLaunch.hpp"
#include "cleanup/Retention.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/IndigenousTime.hpp"
#include "shell/PosixPath.hpp"
#include <chrono>
#include <filesystem>
#include <memory>
#include <regex>
#include <stdexcept>
#include <thread>
#include <ctime>

namespace batbox::cleanup {
namespace {
template<class T,class F>event_loop::Deferred<T> asyncFs(event_loop::Loop& loop,F operation){
  event_loop::Deferred<T> result(loop);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  const auto dispatcher=loop.dispatcher();
  ::batbox::runtime::contextThread([dispatcher,result,keep,operation=std::move(operation)]()mutable{
    try{
      auto value=operation();
      dispatcher.postMission([result,keep,value=std::move(value)]()mutable{
        result.resolve(std::move(value));keep->release();
      });
    }catch(...){
      const auto error=std::current_exception();
      dispatcher.postMission([result,keep,error]{result.reject(error);keep->release();});
    }
  }).detach();
  return result;
}
std::filesystem::path indigenousPath(const std::string& text){
  return runtime::pathFromUtf8(text);
}
}
std::optional<double> indigenousLogDateMs(const std::string& iso){
  static const std::regex full(R"(^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})\.(\d{3})Z$)");
  static const std::regex day(R"(^(\d{4})-(\d{2})-(\d{2})$)");
  std::smatch match;bool withTime=std::regex_match(iso,match,full);
  if(!withTime&&!std::regex_match(iso,match,day))return std::nullopt;
  const auto year=std::stoi(match[1]),month=std::stoi(match[2]),date=std::stoi(match[3]);
  const auto hour=withTime?std::stoi(match[4]):0;
  const auto minute=withTime?std::stoi(match[5]):0;
  const auto second=withTime?std::stoi(match[6]):0;
  const auto millis=withTime?std::stoi(match[7]):0;
  if(month<1||month>12||date<1||date>31||hour>23||minute>59||second>59)
    return std::nullopt;
  std::tm fields{};fields.tm_year=year-1900;fields.tm_mon=month-1;
  fields.tm_mday=date;fields.tm_hour=hour;fields.tm_min=minute;fields.tm_sec=second;
  const auto seconds=runtime::timegmUtc(&fields);
  if(seconds==-1)return std::nullopt;
  std::tm verify{};if(!runtime::gmtimeUtc(seconds,verify))return std::nullopt;
  if(verify.tm_year!=year-1900||verify.tm_mon!=month-1||verify.tm_mday!=date)
    return std::nullopt;
  return static_cast<double>(seconds)*1000.0+millis;
}
RetentionServices indigenousRetentionServices(event_loop::Loop& loop,
  std::function<void(std::exception_ptr)> logError){
  if(!logError)throw std::invalid_argument("local cleanup diagnostic required");
  RetentionServices services;
  services.logError=std::move(logError);
  services.joinPath=shell::joinPosixPath;
  services.parseDateMs=indigenousLogDateMs;
  services.fs.readdir=[&loop](const std::string& dir){
    return asyncFs<std::vector<Dirent>>(loop,[dir]{
      std::vector<Dirent> result;
      for(const auto& entry:std::filesystem::directory_iterator(indigenousPath(dir))){
        const auto type=entry.symlink_status().type();
        result.push_back({runtime::pathToUtf8(entry.path().filename()),
          type==std::filesystem::file_type::regular,
          type==std::filesystem::file_type::directory});
      }
      return result;
    });
  };
  services.fs.unlink=[&loop](const std::string& path){
    return asyncFs<event_loop::Unit>(loop,[path]{
      const auto indigenous=indigenousPath(path);
      if(!std::filesystem::remove(indigenous))
        throw std::filesystem::filesystem_error("unlink",indigenous,
          std::make_error_code(std::errc::no_such_file_or_directory));
      return event_loop::Unit{};
    });
  };
  services.fs.rmdir=[&loop](const std::string& path){
    return asyncFs<event_loop::Unit>(loop,[path]{
      const auto indigenous=indigenousPath(path);
      if(!std::filesystem::remove(indigenous))
        throw std::filesystem::filesystem_error("rmdir",indigenous,
          std::make_error_code(std::errc::no_such_file_or_directory));
      return event_loop::Unit{};
    });
  };
  return services;
}
} // namespace batbox::cleanup

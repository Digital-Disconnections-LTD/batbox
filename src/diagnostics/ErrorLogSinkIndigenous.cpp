#include "diagnostics/ErrorLogSink.hpp"
#include "cleanup/Roster.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace batbox::diagnostics {
ErrorLogServices indigenousErrorLogServices(event_loop::Loop& loop,
    cleanup::Roster& roster,std::shared_ptr<DebugLog> debug,
    std::function<std::string()> errorsDirectory,
    std::function<std::string(const std::string&)> mcpLogsDirectory,
    std::function<std::string()> timestampISO,
    std::function<std::string()> flightId,
    std::function<std::string()> cwd,
    std::function<std::string()> version,
    std::function<void(ErrorLogCallbacks)> attachSink){
  if(!debug)throw std::invalid_argument("Debug logger is required");
  ErrorLogServices services;
  services.loop=&loop;
  services.errorsDirectory=std::move(errorsDirectory);
  services.mcpLogsDirectory=std::move(mcpLogsDirectory);
  services.timestampISO=std::move(timestampISO);
  services.flightId=std::move(flightId);
  services.cwd=std::move(cwd);
  services.version=std::move(version);
  services.isInternalUser=[](){
    const auto* value=std::getenv("BATBOX_INTERNAL_USER");
    return value&&*value&&std::string(value)!="0";
  };
  services.internalUserTypeLabel=[]{return "batbox-internal";};
  services.logForDebugging=[debug=std::move(debug)](
      const std::string& message,const std::string& level){
    debug->logForDebugging(message,level);
  };
  services.appendFileSync=[](const std::string& path,const std::string& value){
    std::ofstream out(path,std::ios::binary|std::ios::app);
    if(!out)throw std::runtime_error("Cannot open error log");
    out.write(value.data(),static_cast<std::streamsize>(value.size()));
    if(!out)throw std::runtime_error("Cannot append error log");
  };
  services.mkdirSync=[](const std::string& path){
    if(!std::filesystem::create_directory(path))
      throw std::system_error(std::make_error_code(std::errc::file_exists),"mkdir");
  };
  services.registerCleanup=[&loop,&roster](std::function<void()> callback){
    roster.registerCleanup([&loop,callback=std::move(callback)]{
      DebugMission result(loop);
      try{callback();result.resolve({});}
      catch(...){result.reject(std::current_exception());}
      return result;
    });
  };
  services.attachErrorLogSink=std::move(attachSink);
  return services;
}
}

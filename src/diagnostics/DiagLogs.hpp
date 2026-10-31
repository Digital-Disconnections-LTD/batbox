#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>

namespace batbox::diagnostics {
struct Services {
  std::function<std::optional<std::string>()> diagnosticFile;
  std::function<std::string()> timestamp;
  std::function<double()> nowMs;
  std::function<void(const std::string&,const std::string&)> appendFileSync;
  std::function<void(const std::string&)> mkdirSync;
};

class Logger {
public:
  explicit Logger(Services services);
  void logForDiagnosticsNoPII(const std::string& level,
      const std::string& event,const Json& data=Json::object()) const;
  double nowMs() const{return services_.nowMs();}
private:
  Services services_;
};

Services indigenousDiagnosticServices(std::function<std::string()> timestamp,
    std::function<double()> nowMs);

template<class T>
event_loop::Deferred<T> withDiagnosticsTiming(event_loop::Loop& loop,
    std::shared_ptr<Logger> logger,const std::string& event,
    std::function<event_loop::Deferred<T>()> operation,
    std::function<Json(const T&)> getData={}){
  if(!logger||!operation)throw std::invalid_argument("Diagnostic timing imports are required");
  const auto start=logger->nowMs();
  logger->logForDiagnosticsNoPII("info",event+"_started");
  event_loop::Deferred<T> result(loop);
  const auto fail=[logger,event,start,result](std::exception_ptr error){
    logger->logForDiagnosticsNoPII("error",event+"_failed",
      Json{{"duration_ms",logger->nowMs()-start}});
    result.reject(error);
  };
  try{
    auto pending=operation();
    pending.then([logger,event,start,result,fail,getData](const T* value,
        const std::exception_ptr& error){
      if(error){fail(error);return;}
      try{
        Json data={{"duration_ms",logger->nowMs()-start}};
        if(getData){
          const auto extra=getData(*value);
          if(extra.is_object())for(auto it=extra.begin();it!=extra.end();++it)
            data[it.key()]=it.value();
        }
        logger->logForDiagnosticsNoPII("info",event+"_completed",data);
        result.resolve(*value);
      }catch(...){fail(std::current_exception());}
    });
  }catch(...){fail(std::current_exception());}
  return result;
}

} // namespace batbox::diagnostics

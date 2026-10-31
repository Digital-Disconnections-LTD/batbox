#include "diagnostics/DiagLogs.hpp"
#include "runtime/Paths.hpp"
#include <system_error>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace batbox::diagnostics {
Logger::Logger(Services services):services_(std::move(services)){
  if(!services_.diagnosticFile||!services_.timestamp||!services_.nowMs||
      !services_.appendFileSync||!services_.mkdirSync)
    throw std::invalid_argument("Diagnostic logger imports are required");
}
void Logger::logForDiagnosticsNoPII(const std::string& level,
    const std::string& event,const Json& data) const{
  const auto path=services_.diagnosticFile();
  if(!path||path->empty())return;
  const Json entry={{"timestamp",services_.timestamp()},
    {"level",level},{"event",event},
    {"data",data.is_null()?Json::object():data}};
  const auto line=value::stringify(entry)+"\n";
  try{services_.appendFileSync(*path,line);}
  catch(...){
    try{
      services_.mkdirSync(value::dirname(*path));
      services_.appendFileSync(*path,line);
    }catch(...){}
  }
}
Services indigenousDiagnosticServices(std::function<std::string()> timestamp,
    std::function<double()> nowMs){
  Services services;
  services.diagnosticFile=[]{
    const auto* value=std::getenv("BATBOX_DIAGNOSTICS_FILE");
    return value&&*value?std::optional(std::string(value)):std::nullopt;
  };
  services.timestamp=std::move(timestamp);
  services.nowMs=std::move(nowMs);
  services.appendFileSync=[](const std::string& path,const std::string& line){
    std::ofstream output(path,std::ios::binary|std::ios::app);
    if(!output)throw std::runtime_error("Cannot append diagnostic log");
    output<<line;
    if(!output)throw std::runtime_error("Cannot append diagnostic log");
  };
  services.mkdirSync=[](const std::string& path){
    if(!std::filesystem::create_directory(path))
      throw std::system_error(std::make_error_code(std::errc::file_exists),"mkdir");
  };
  return services;
}
}

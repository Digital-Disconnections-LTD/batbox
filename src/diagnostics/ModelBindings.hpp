#pragma once
#include "diagnostics/DebugLog.hpp"
#include "diagnostics/DiagLogs.hpp"
#include "model/QueryModel.hpp"
#include <memory>

namespace batbox::diagnostics {
inline void bindModelLocalLogs(model::QueryServices& services,
    std::shared_ptr<DebugLog> debug,std::shared_ptr<Logger> diagnostic){
  if(!debug||!diagnostic)
    throw std::invalid_argument("Local model loggers are required");
  services.logForDebugging=[debug=std::move(debug)](
      const std::string& message,const std::string& level){
    debug->logForDebugging(message,level);
  };
  services.logForDiagnosticsNoPII=[diagnostic=std::move(diagnostic)](
      const std::string& level,const std::string& event){
    diagnostic->logForDiagnosticsNoPII(level,event);
  };
}
}

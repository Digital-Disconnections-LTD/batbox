#include "diagnostics/ExecSyncWrapper.hpp"
#include "diagnostics/SlowOperations.hpp"
#include <exception>
#include <stdexcept>

namespace batbox::diagnostics {
ExecSyncResult execSyncDeprecated(const std::string& command,
    std::optional<Json> options,const ExecSyncImports& imports){
  if(!imports.beginSlowLogging||!imports.nodeExecSync)
    throw std::invalid_argument("execSync wrapper imports are required");
  auto dispose=imports.beginSlowLogging({"execSync: ",""},
    {Json(value::slice(command,0,100))});
  if(!dispose)throw std::invalid_argument("slowLogging disposer is required");
  ExecSyncResult result;
  try{
    result=imports.nodeExecSync(command,options);
  }catch(...){
    const auto original=std::current_exception();
    try{dispose();}
    catch(...){throw SuppressedOperationError(std::current_exception(),original);}
    std::rethrow_exception(original);
  }
  dispose();
  return result;
}
}

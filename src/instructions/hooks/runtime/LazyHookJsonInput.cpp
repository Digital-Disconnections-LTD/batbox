#include "instructions/hooks/runtime/LazyHookJsonInput.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
LazyHookJsonInput::LazyHookJsonInput(Json input,std::string hookName,
  std::function<std::string(const Json&)> stringify,
  std::function<void(const std::string&,std::exception_ptr)> logError):
  input_(std::move(input)),hookName_(std::move(hookName)),
  stringify_(std::move(stringify)),logError_(std::move(logError)){}
const HookJsonInputResult& LazyHookJsonInput::get(){
  if(result_)return *result_;
  try{
    if(!stringify_)throw std::invalid_argument("Hook JSON stringify import required");
    result_=HookJsonInputResult{true,stringify_(input_),{}};
  }catch(...){
    const auto error=std::current_exception();
    if(!logError_)throw std::invalid_argument("Hook local error logger required");
    // Preserve the cause as exception_ptr for the local diagnostics sink.
    logError_("Failed to stringify hook "+hookName_+" input",error);
    result_=HookJsonInputResult{false,{},error};
  }
  return *result_;
}
} // namespace batbox::instructions::hooks::runtime

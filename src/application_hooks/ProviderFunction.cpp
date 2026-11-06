#include "application_hooks/Provider.hpp"
#include "model/QueryModel.hpp"
#include <stdexcept>

namespace batbox::application_hooks {
namespace {
std::string errorText(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& value){return value.what();}
  catch(...){return "unknown function hook error";}
  return {};
}
}
void Provider::bindFunctionHooks(){
  hooks::FunctionHookServices services;
  services.logError=[log=inputs_.log](std::exception_ptr error){
    log("Function hook execution failed: "+errorText(error));};
  services.createAttachmentMessage=inputs_.createAttachmentMessage;
  services.isErrorInstance=[](std::exception_ptr error){
    try{if(error)std::rethrow_exception(error);}
    catch(const std::exception&){return true;}
    catch(...){return false;}
    return false;};
  services.isAbortError=[](std::exception_ptr error){
    try{if(error)std::rethrow_exception(error);}
    catch(const model::UserAbortError&){return true;}
    catch(...){return false;}
    return false;};
  services.errorMessage=errorText;
  functionHost_=std::make_shared<hooks::FunctionHookHost>(owner_,
    std::move(services));
}
} // namespace batbox::application_hooks

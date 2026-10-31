#include "auth/helper/Helper.hpp"
#include <stdexcept>

namespace batbox::auth::helper {
namespace {
std::string errorMessage(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const std::exception& caught){return caught.what();}
  catch(...){return "unknown error";}
}
}
event_loop::Deferred<std::optional<std::string>> Stash::executeHelper(
    bool isNonInteractiveFlight){
  event_loop::Deferred<std::optional<std::string>> output(loop_);
  try{
    const auto helper=services_.configuredHelper();
    if(!value::truthy(helper)){
      output.resolve(std::nullopt);return output;
    }
    if(isHelperFromProjectOrLocalSettings()){
      const auto trusted=services_.trustAccepted();
      if(!trusted&&!isNonInteractiveFlight){
        services_.trustError("apiKeyHelper invoked before trust check",
          "Security: apiKeyHelper executed before workspace trust is "
          "confirmed. If you see this message, post in "+
          services_.feedbackChannel+".");
        output.resolve(std::nullopt);return output;
      }
    }
    services_.executeShell(helper.get<std::string>(),10*60*1000).then(
      [output](const CommandResult* result,const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        try{
          if(result->failed){
            const auto why=result->timedOut?std::string("timed out"):
              "exited "+std::to_string(result->exitCode);
            const auto errorText=value::trim(result->stderrText);
            throw std::runtime_error(errorText.empty()?why:why+": "+errorText);
          }
          const auto outputText=value::trim(result->stdoutText);
          if(outputText.empty())throw std::runtime_error("did not return a value");
          output.resolve(outputText);
        }catch(...){output.reject(std::current_exception());}
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<std::optional<std::string>> Stash::runAndStash(
    bool isNonInteractiveFlight,bool isCold,std::uint64_t epoch){
  event_loop::Deferred<std::optional<std::string>> output(loop_);
  executeHelper(isNonInteractiveFlight).then(
    [this,output,isCold,epoch](const std::optional<std::string>* value,
        const std::exception_ptr& error){
      try{
        if(error){
          if(epoch!=epoch_){output.resolve(std::string(" "));return;}
          const auto detail=errorMessage(error);
          services_.consoleError("apiKeyHelper failed: "+detail);
          services_.debug("Error getting API key from apiKeyHelper: "+
            detail,"error");
          if(!isCold&&stashed_&&stashed_->value!=" "){
            stashed_->timestamp=services_.nowMs();
            output.resolve(stashed_->value);return;
          }
          stashed_=Stashed{" ",services_.nowMs()};
          output.resolve(std::string(" "));return;
        }
        if(epoch==epoch_&&*value)
          stashed_=Stashed{**value,services_.nowMs()};
        output.resolve(*value);
      }catch(...){output.reject(std::current_exception());}
      if(epoch==epoch_)inflight_.reset();
    });
  return output;
}
}

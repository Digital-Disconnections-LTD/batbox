#include "terminal_runtime/TerminalControls.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::terminal_runtime {
std::function<void()> TerminalControls::patchStderr(
    std::shared_ptr<StderrPort> stderr){
  if(!stderr)throw std::invalid_argument("Stderr port required");
  const auto originalWrite=stderr->writeMethod();
  if(!originalWrite)throw std::invalid_argument("Stderr write required");
  auto reentered=std::make_shared<bool>(false);
  auto self=shared_from_this();
  auto intercept=std::make_shared<StderrWrite>(
    [self,originalWrite,reentered](const StderrWriteCall& call)->bool{
      if(*reentered)return (*originalWrite)(call);
      *reentered=true;
      try{
        std::string value;
        if(const auto* string=std::get_if<std::string>(&call.chunk))
          value=*string;
        else{
          const auto& bytes=std::get<std::vector<std::uint8_t>>(call.chunk);
          value=value::decodeUtf8(std::string_view(
            reinterpret_cast<const char*>(bytes.data()),bytes.size()));
        }
        self->services_.logForDebugging("[stderr] "+value,"warn");
        if(self->altScreenActive_&&!self->isUnmounted_&&!self->isPaused_){
          self->prevFrameContaminated_=true;
          if(!self->interaction_||!self->interaction_->scheduleRender)
            throw std::logic_error("Render scheduler import required");
          self->interaction_->scheduleRender();
        }
      }catch(...){
        *reentered=false;
        if(call.callback)call.callback();
        throw;
      }
      *reentered=false;
      if(call.callback)call.callback();
      return true;
    });
  stderr->setWriteMethod(intercept);
  return [stderr,originalWrite,intercept]{
    if(stderr->writeMethod()==intercept)
      stderr->setWriteMethod(originalWrite);
  };
}
} // namespace batbox::terminal_runtime

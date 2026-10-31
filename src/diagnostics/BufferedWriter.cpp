#include "diagnostics/BufferedWriter.hpp"
#include "runtime/JsonValue.hpp"
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace batbox::diagnostics {
namespace {
std::string join(const std::vector<std::string>& values){
  std::string result;for(const auto& value:values)result+=value;return result;
}
}
struct BufferedWriter::State:std::enable_shared_from_this<State> {
  BufferedWriterServices services;
  std::vector<std::string> buffer;
  double bufferBytes=0;
  std::function<void()> flushTimer;
  std::optional<std::vector<std::string>> pendingOverflow;
  explicit State(BufferedWriterServices value):services(std::move(value)){}
  void clearTimer(){
    if(flushTimer){auto cancel=std::move(flushTimer);flushTimer={};cancel();}
  }
  void flush(){
    if(pendingOverflow){
      services.writeFn(join(*pendingOverflow));
      pendingOverflow.reset();
    }
    if(buffer.empty())return;
    services.writeFn(join(buffer));
    buffer.clear();bufferBytes=0;
    clearTimer();
  }
  void scheduleFlush(){
    if(flushTimer)return;
    auto self=shared_from_this();
    flushTimer=services.setTimeout(services.flushIntervalMs,[self]{self->flush();});
  }
  void flushDeferred(){
    if(pendingOverflow){
      pendingOverflow->insert(pendingOverflow->end(),buffer.begin(),buffer.end());
      buffer.clear();bufferBytes=0;clearTimer();return;
    }
    auto detached=std::move(buffer);
    buffer.clear();bufferBytes=0;clearTimer();
    pendingOverflow=std::move(detached);
    auto self=shared_from_this();
    services.setImmediate([self]{
      auto toWrite=std::move(self->pendingOverflow);
      self->pendingOverflow.reset();
      if(toWrite)self->services.writeFn(join(*toWrite));
    });
  }
  void write(const std::string& content){
    if(services.immediateMode){services.writeFn(content);return;}
    buffer.push_back(content);
    bufferBytes+=value::stringLength(content);
    scheduleFlush();
    if(buffer.size()>=services.maxBufferSize||bufferBytes>=services.maxBufferBytes)
      flushDeferred();
  }
};
BufferedWriter::BufferedWriter(BufferedWriterServices services){
  if(!services.writeFn||!services.setTimeout||!services.setImmediate)
    throw std::invalid_argument("Buffered writer imports are required");
  state_=std::make_shared<State>(std::move(services));
}
void BufferedWriter::write(const std::string& content){state_->write(content);}
void BufferedWriter::flush(){state_->flush();}
void BufferedWriter::dispose(){state_->flush();}
BufferedWriterServices indigenousBufferedWriterServices(event_loop::Loop& loop,
    std::function<void(const std::string&)> writeFn){
  BufferedWriterServices services;
  services.writeFn=std::move(writeFn);
  services.setTimeout=[&loop](double ms,std::function<void()> callback){
    const auto timer=loop.setTimeout(ms,std::move(callback));
    return [timer]{timer.cancel();};
  };
  services.setImmediate=[&loop](std::function<void()> callback){
    loop.postMission(std::move(callback));
  };
  return services;
}
}

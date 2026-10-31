#include "query/OwnerStreamConsume.hpp"
#include "query/OwnerSegmentInvoker.hpp"
#include "model/QueryModel.hpp"
#include "model/StreamObservation.hpp"

namespace batbox::query {
void consumeOwnerStream(const event_loop::Loop::Dispatcher& owner,
    std::stop_token cursorClose,model::OpenedStream& opened,
    streaming::Reducer& reducer,
    const std::function<void(const streaming::MessageRef&)>& yield,
    model::StreamWatchdog* watchdog,
    const std::function<void()>& onFirstChunk,
    model::StreamObservation* observation){
  try{
    bool isFirstChunk=true;
    while(auto event=opened.next()){
      if(watchdog)watchdog->reset();
      auto emission=invokeOwnerSegment(owner,cursorClose,[&]{
        if(observation)observation->onEvent();
        if(isFirstChunk){
          if(onFirstChunk)onFirstChunk();
          isFirstChunk=false;
        }
        return reducer.beginEvent(*event);
      });
      while(emission){
        yield(*emission);
        if(cursorClose.stop_requested())throw model::GeneratorClosed{};
        emission=invokeOwnerSegment(owner,cursorClose,[&]{
          return reducer.nextEmission();
        });
      }
    }
    if(watchdog){
      watchdog->clear();
      if(watchdog->aborted()){
        (void)invokeOwnerSegment(owner,cursorClose,[&]{
          if(observation)observation->watchdogCleanExit();
          return event_loop::Unit{};
        });
        throw std::runtime_error("Stream idle timeout - no chunks received");
      }
    }
    (void)invokeOwnerSegment(owner,cursorClose,[&]{
      if(observation)observation->beforeFinish(reducer);
      reducer.finish();return event_loop::Unit{};
    });
  }catch(...){
    if(watchdog)watchdog->clear();
    if(opened.abort)opened.abort();
    throw;
  }
  if(opened.abort)opened.abort();
}
} // namespace batbox::query

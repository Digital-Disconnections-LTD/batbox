#include "runtime/AsyncLaunch.hpp"
#include "model_stash/AsyncDetection.hpp"
#include <stdexcept>
#include <thread>

namespace batbox::model_stash {
namespace {
std::optional<std::string> batIdOf(const Json& value){
  return value.is_string()?std::optional(value.get<std::string>()):std::nullopt;
}
}

StashCheckCallback makeAsyncStashCheck(event_loop::Loop& loop,
    std::shared_ptr<Detector> detector,DiffWriterServices writer){
  if(!detector)throw std::invalid_argument("Cache detector is required");
  const auto dispatcher=loop.dispatcher();
  return [&loop,dispatcher,detector=std::move(detector),writer=std::move(writer)](
      const std::string& source,const Json& read,const Json& creation,
      const Json& messages,const Json& batId,
      const std::optional<std::string>& requestId){
    if(!dispatcher.isOwnerThread())
      throw std::logic_error("Cache response check requires query owner");
    auto pending=detector->beginResponseCheck(source,value::numberOr(read),
      value::numberOr(creation),messages,batIdOf(batId),requestId);
    if(!pending)return;
    auto keepAlive=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
    ::batbox::runtime::contextThread([dispatcher,detector,writer,pending=std::move(*pending),keepAlive]{
      const auto path=writeStashBreakDiff(pending.previousContent,
        pending.currentContent,writer);
      try{
        dispatcher.postMission([dispatcher,detector,pending,path,keepAlive]{
          dispatcher.queueMicrotask([detector,pending,path,keepAlive]{
            detector->finishResponseCheck(pending,path);
            keepAlive->release();
          });
        });
      }catch(...){keepAlive->release();}
    }).detach();
  };
}

} // namespace batbox::model_stash

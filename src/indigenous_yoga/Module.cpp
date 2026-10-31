#include "indigenous_yoga/Module.hpp"

namespace batbox::indigenous_yoga {
YogaModuleRef yogaInstance(){
  static auto instance=std::make_shared<YogaModule>();
  return instance;
}
event_loop::Deferred<YogaModuleRef> loadYoga(event_loop::Loop& loop){
  event_loop::Deferred<YogaModuleRef> result(loop);
  result.resolve(yogaInstance());
  return result;
}
} // namespace batbox::indigenous_yoga

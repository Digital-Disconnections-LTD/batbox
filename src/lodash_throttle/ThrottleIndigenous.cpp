#include "lodash_throttle/Throttle.hpp"
#include <chrono>

namespace batbox::lodash_throttle {
Services indigenousServices(event_loop::Loop& loop){
  return {
    []{
      using Clock=std::chrono::system_clock;
      return static_cast<double>(std::chrono::duration_cast<
        std::chrono::milliseconds>(Clock::now().time_since_epoch()).count());
    },
    [&loop](double delay,std::function<void()> callback){
      const auto handle=loop.setTimeout(delay,std::move(callback));
      return Timer{[handle]{handle.cancel();}};
    }
  };
}
} // namespace batbox::lodash_throttle

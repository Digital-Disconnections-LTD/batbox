#include "bootstrap/Interaction.hpp"
#include "bootstrap/IndigenousCosts.hpp"
#include <utility>

namespace batbox::bootstrap {
InteractionServices makeIndigenousInteractionServices(event_loop::Loop& loop){
  return {indigenousNowMs,[&loop](double delayMs,std::function<void()> callback){
    auto handle=loop.setTimeout(delayMs,std::move(callback));
    return InteractionTimer{
      [handle]{handle.cancel();},[handle]{handle.unref();}};
  }};
}
}

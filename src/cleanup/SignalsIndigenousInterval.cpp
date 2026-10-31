#include "cleanup/Signals.hpp"
#include <memory>

namespace batbox::cleanup {
ShutdownTimer indigenousUnrefInterval(event_loop::Loop& loop,double delayMs,
  std::function<void()> callback){
  struct State {
    event_loop::Loop& loop;
    double delay;
    std::function<void()> callback;
    event_loop::Loop::TimerHandle current;
    bool active=true,referenced=true;
    std::function<void()> schedule;
    State(event_loop::Loop& owner,double ms,std::function<void()> fn):
      loop(owner),delay(ms),callback(std::move(fn)){}
  };
  auto state=std::make_shared<State>(loop,delayMs,std::move(callback));
  std::weak_ptr<State> weak=state;
  state->schedule=[weak]{
    auto state=weak.lock();if(!state||!state->active)return;
    state->current=state->loop.setTimeout(state->delay,[weak]{
      auto state=weak.lock();if(!state||!state->active)return;
      state->callback();
      if(state->active)state->schedule();
    },state->referenced);
  };
  state->schedule();
  return {
    [state]{state->active=false;state->current.cancel();state->schedule={};},
    [state]{state->referenced=false;state->current.unref();}
  };
}
} // namespace batbox::cleanup

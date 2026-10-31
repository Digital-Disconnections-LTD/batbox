#include "cleanup/IndigenousSignalBridge.hpp"
#include "cleanup/Signals.hpp"
#include <cassert>
#include <chrono>
#include <csignal>
#include <thread>
#include <unistd.h>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  int signals=0;
  {
    cleanup::IndigenousSignalBridge bridge(loop);
    auto keep=loop.hold();
    bridge.onSignal(SIGUSR1,[&]{
      assert(loop.isOwnerThread());++signals;keep.release();
    });
    std::jthread sender([]{
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      ::kill(::getpid(),SIGUSR1);
    });
    loop.run();
    assert(signals==1);
  }
  int ticks=0;
  auto interval=cleanup::indigenousUnrefInterval(loop,5,[&]{++ticks;});
  interval.unref();
  loop.run(); // The unreferenced orphan check cannot keep the process alive.
  assert(ticks==0);
  auto keep=loop.hold();
  auto stop=loop.setTimeout(20,[&]{keep.release();});
  loop.run();
  assert(ticks>=1);
  interval.cancel();stop.cancel();
}

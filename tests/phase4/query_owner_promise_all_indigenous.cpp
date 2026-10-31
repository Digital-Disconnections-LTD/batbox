#include "query/OwnerPromiseAll.hpp"
#include <cassert>
#include <vector>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  event_loop::Deferred<int> slow(loop),fast(loop);
  auto combined=query::ownerPromiseAll(loop,std::vector{slow,fast});
  std::vector<int> ordered;
  combined.then([&](const std::vector<int>* result,
      const std::exception_ptr& error){
    assert(!error&&result);ordered=*result;loop.stop();
  });
  auto fastTimer=loop.setTimeout(1,[&]{fast.resolve(2);});
  auto slowTimer=loop.setTimeout(20,[&]{slow.resolve(1);});
  auto hold=loop.hold();loop.run();hold.release();
  assert(ordered==std::vector<int>({1,2}));

  std::vector<std::string> trace;
  auto empty=query::ownerPromiseAll<int>(loop,{});
  empty.then([&](const std::vector<int>* value,const std::exception_ptr& error){
    assert(!error&&value&&value->empty());trace.push_back("then");});
  trace.push_back("after-register");
  loop.runReady();
  assert(trace==std::vector<std::string>({"after-register","then"}));
}

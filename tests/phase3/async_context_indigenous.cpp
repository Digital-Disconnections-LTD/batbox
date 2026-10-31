#include "runtime/AsyncLocalStorage.hpp"
#include "event_loop/Deferred.hpp"
#include <cassert>
#include <thread>
struct Data {int value=0;};
int main(){using namespace batbox;
  event_loop::Loop loop;runtime::AsyncLocalStorage<Data> store;
  std::weak_ptr<Data> weak;bool promise=false,timer=false;
  event_loop::Deferred<event_loop::Unit> output(loop);
  auto value=std::make_shared<Data>(Data{7});weak=value;
  store.run(value,[&]{
    output.then([&](const event_loop::Unit*,const std::exception_ptr& error){
      assert(!error&&store.getStore()==weak.lock());
      assert(store.getStore()->value==9);promise=true;
    });
    loop.setTimeout(1,[&]{assert(store.getStore()==weak.lock());timer=true;});
  });
  value->value=9;value.reset();assert(!weak.expired()&&!store.getStore());
  std::thread worker([&]{assert(!store.getStore());output.resolve({});});
  worker.join();loop.run();
  assert(promise&&timer&&!store.getStore()&&weak.expired());
}

#include "runtime/AsyncLocalStorage.hpp"
#include "runtime/AsyncLaunch.hpp"
#include "cwd/Override.hpp"
#include <cassert>
#include <future>
int main(){using namespace batbox;
 runtime::AsyncLocalStorage<std::string> store;const auto global=[]{return std::string("/global");};
 const auto alpha=std::make_shared<std::string>("alpha"),beta=std::make_shared<std::string>("beta");
 std::future<std::string> delayed,active;std::thread worker;std::promise<std::string> result;
 store.run(alpha,[&]{cwd::runWithCwdOverride("/alpha",[&]{
   delayed=runtime::contextAsync(std::launch::deferred,[&]{return *store.getStore()+cwd::pwd(global);});
   active=runtime::contextAsync(std::launch::async,[&]{return *store.getStore()+cwd::pwd(global);});
   worker=runtime::contextThread([&]{result.set_value(*store.getStore()+cwd::pwd(global));});
 });});
 store.run(beta,[&]{cwd::runWithCwdOverride("/beta",[&]{assert(delayed.get()=="alpha/alpha");});});
 assert(active.get()=="alpha/alpha");worker.join();assert(result.get_future().get()=="alpha/alpha");
 assert(!store.getStore()&&cwd::pwd(global)=="/global");
}

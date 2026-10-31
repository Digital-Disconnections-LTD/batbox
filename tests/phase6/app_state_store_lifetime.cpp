#include "app_state/Store.hpp"
#include <cassert>
#include <memory>

using Store=batbox::app_state::Store<int>;
int main(){
  {
    Store store(0);
    auto listener=std::make_shared<Store::Listener>([]{});
    std::weak_ptr<Store::Listener> weak=listener;
    auto unsubscribe=store.subscribe(listener);
    listener.reset();
    unsubscribe();
    assert(weak.expired());
  }
  {
    std::weak_ptr<Store::Listener> weak;
    std::function<void()> unsubscribe;
    {
      Store store(0);
      auto listener=std::make_shared<Store::Listener>([]{});
      weak=listener;
      unsubscribe=store.subscribe(listener);
    }
    assert(weak.expired());
    unsubscribe();
  }
}

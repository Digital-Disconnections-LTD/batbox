#include "bats/BatGadgetRace.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
using namespace std::chrono_literals;
int main(){
  using namespace batbox;
  auto ref=std::make_shared<Json>(Json{{"type","assistant"}});
  {
    std::promise<std::optional<query::MessageRef>> message;
    std::promise<void> background;
    auto messageFuture=message.get_future().share();
    auto backgroundFuture=background.get_future().share();
    message.set_value(ref);background.set_value();
    const auto result=bats::raceBatMessageAndBackground(messageFuture,backgroundFuture);
    assert(!result.background && result.message && *result.message==ref);
  }
  {
    std::promise<std::optional<query::MessageRef>> message;
    std::promise<void> background;
    auto messageFuture=message.get_future().share();
    auto backgroundFuture=background.get_future().share();
    background.set_value();
    const auto result=bats::raceBatMessageAndBackground(messageFuture,backgroundFuture);
    assert(result.background);
    message.set_value(std::nullopt);
  }
  {
    std::promise<std::optional<query::MessageRef>> message;
    auto messageFuture=message.get_future().share();
    std::thread producer([&]{std::this_thread::sleep_for(3ms);message.set_value(ref);});
    const auto result=bats::raceBatMessageAndBackground(messageFuture,std::nullopt);
    assert(!result.background && result.message && *result.message==ref);
    producer.join();
  }
  std::cout<<"BatGadget Promise.race ordering and readiness passed\n";
}

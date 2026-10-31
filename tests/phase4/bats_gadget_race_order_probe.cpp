#include "bats/BatGadgetRace.hpp"
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line)){
    const auto input=value::parse(line);
    const auto scenario=input.at("case").get<std::string>();
    auto messageSignal=std::make_shared<bats::CompletionSignal>();
    auto backgroundSignal=std::make_shared<bats::CompletionSignal>();
    backgroundSignal->preRegisterMapping();
    std::promise<std::optional<query::MessageRef>> messagePromise;
    std::promise<void> backgroundPromise;
    auto messageFuture=messagePromise.get_future().share();
    auto backgroundFuture=backgroundPromise.get_future().share();
    auto settleMessage=[&](){
      messageSignal->settle();
      messagePromise.set_value(std::make_shared<Json>(Json{{"type","assistant"}}));
    };
    auto settleBackground=[&](){backgroundSignal->settle();backgroundPromise.set_value();};
    if(scenario=="both" || scenario=="both_flushed" || scenario=="msg_ready")
      settleMessage();
    if(scenario=="both" || scenario=="both_flushed" || scenario=="bg_ready")
      settleBackground();
    auto racer=std::async(std::launch::async,[&](){
      return bats::raceBatMessageAndBackground(
        bats::ObservedBatMessage{messageFuture,messageSignal},
        bats::ObservedBackgroundSignal{backgroundFuture,backgroundSignal});
    });
    if(scenario=="bg_first" || scenario=="msg_first" ||
       scenario=="msg_ready" || scenario=="bg_ready"){
      const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
      while((messageSignal->subscriberCount()==0 ||
             backgroundSignal->subscriberCount()==0) &&
            std::chrono::steady_clock::now()<deadline)
        std::this_thread::yield();
      if(scenario=="bg_first"){settleBackground();settleMessage();}
      else if(scenario=="msg_first"){settleMessage();settleBackground();}
      else if(scenario=="msg_ready")settleBackground();
      else settleMessage();
    }
    const auto result=racer.get();
    std::cout<<value::stringify(Json{{"case",scenario},
      {"winner",result.background?"background":"message"}})<<'\n';
  }
}

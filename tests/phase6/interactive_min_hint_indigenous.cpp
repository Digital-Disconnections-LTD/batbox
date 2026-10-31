#include "interactive/cards_collapsed/MinHintDisplay.hpp"
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  const auto start=std::chrono::steady_clock::now();
  auto now=[&]{return 100000.0+
    std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now()-start).count();};
  int changes=0;
  auto hint=interactive::cards_collapsed::MinHintDisplay::create(
    loop,now,[&]{++changes;});
  std::vector<std::string> values;
  auto render=[&](const std::string& value){
    values.push_back(hint->render(value,100).value_or(""));
    hint->committed(value,100);
  };
  render("A");std::this_thread::sleep_for(std::chrono::milliseconds(10));
  render("B");std::this_thread::sleep_for(std::chrono::milliseconds(10));
  render("C");std::this_thread::sleep_for(std::chrono::milliseconds(150));
  loop.runReady();render("C");
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  render("D");render("D");
  std::cout<<batbox::Json{{"values",values},
                          {"final",hint->render("D",100).value_or("")}}.dump()<<'\n';
  hint->cancel();
  if(changes!=2)return 2;
}

#include <cstdlib>
#include "shell/MissionOutputPoller.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
int main(){
  char temp[]="/tmp/batbox-poller-order-XXXXXX";if(!::mkdtemp(temp))return 1;
  const std::filesystem::path root=temp;
  try{
    const auto a=root/"a",b=root/"b";
    {std::ofstream out(a);out<<"alpha\n";}
    {std::ofstream out(b);out<<"beta\n";}
    std::vector<std::string> order;
    batbox::shell::MissionOutputPoller poller;
    poller.registerOutput("a",a.string(),[&](const auto& sample){
      if(sample.lastLines!="alpha\n")throw std::runtime_error("a tail wrong");
      order.push_back("a");});
    poller.registerOutput("b",b.string(),[&](const auto& sample){
      if(sample.lastLines!="beta\n")throw std::runtime_error("b tail wrong");
      order.push_back("b");});
    poller.startPolling("b");poller.startPolling("a");poller.tick();
    if(order!=std::vector<std::string>{"b","a"})
      throw std::runtime_error("active polling insertion order changed");
    order.clear();poller.stopPolling("b");poller.tick();
    if(order!=std::vector<std::string>{"a"})
      throw std::runtime_error("stopPolling did not remove only selected task");
    poller.stopPolling("a");poller.unregisterOutput("a");poller.unregisterOutput("b");
    std::filesystem::remove_all(root);std::cout<<"PASS shared poller active order/stop\n";return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';
    std::filesystem::remove_all(root);return 1;}
}

#include "connectors/Scrapling.hpp"
#include "connectors/Searxng.hpp"
#include <chrono>
#include <iostream>
#include <thread>
using namespace batbox;
int main(int argc,char** argv) {
  if(argc!=3)return 1;
  try {
    std::stop_source searchStop;
    std::jthread cancelSearch([&]{std::this_thread::sleep_for(std::chrono::milliseconds(50));searchStop.request_stop();});
    bool aborted=false;
    try{Searxng(argv[2]).search({{"query","delayed search"}},"cancel-search",std::chrono::seconds(3),searchStop.get_token());}
    catch(const ConnectorAbortError&){aborted=true;}
    if(!aborted)throw std::runtime_error("Search cancellation was lost");
    Scrapling client({"python3",argv[1]},std::chrono::seconds(3));
    const auto initial=client.callGadget("cancel_status",Json::object());
    std::stop_source stop;
    std::jthread cancel([&]{std::this_thread::sleep_for(std::chrono::milliseconds(50));stop.request_stop();});
    aborted=false;
    try{client.callGadget("slow_reply",Json::object(),stop.get_token());}
    catch(const ConnectorAbortError&){aborted=true;}
    if(!aborted)throw std::runtime_error("MCP cancellation was lost");
    const auto status=client.callGadget("cancel_status",Json::object());
    if(status.at("pid")!=initial.at("pid") || status.at("cancelled").size()!=1)
      throw std::runtime_error("Cancellation lost the MCP process/session or notification");
    if(client.callGadget("after_delay",Json::object()).at("after")!=true)
      throw std::runtime_error("A cancelled request's late response corrupted the next call");
    if(client.callGadget("ping_then_reply",Json::object()).at("ping")!="okay")
      throw std::runtime_error("Server ping was not answered");
    const auto before=client.callGadget("cancel_status",Json::object());
    try{client.listGadgets(stop.get_token());throw std::runtime_error("Pre-cancelled request was sent");}
    catch(const ConnectorAbortError&){}
    if(client.callGadget("cancel_status",Json::object())!=before)
      throw std::runtime_error("Pre-cancellation changed server state");
    std::cout<<"Search/MCP cancellation, live session, late replies and server ping passed\n";
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}

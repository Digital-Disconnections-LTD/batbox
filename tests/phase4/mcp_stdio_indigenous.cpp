#include "mcp/StdioClient.hpp"
#include "connectors/Abort.hpp"
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <stop_token>
#include <thread>

int main(int argc,char** argv){
  assert(argc==2);
  auto client=std::make_shared<batbox::mcp::StdioClient>(
    std::vector<std::string>{"python3",argv[1]},"fixture",std::chrono::seconds(3));
  assert(client->hasGadgetCapability());
  const auto listing=client->listGadgets();
  assert(listing["tools"].size()==3);
  std::vector<batbox::Json> progress;std::mutex mutex;
  const auto report=[&](const batbox::Json& item){std::lock_guard lock(mutex);progress.push_back(item);};
  const auto echo=client->callGadget("echo",{{"text","hello"}}, {},report);
  assert(echo.content[0]["text"]=="hello");
  assert(echo.content[1]["type"]=="resource_link");
  assert(echo.meta["origin"]=="fixture");
  assert(echo.structuredContent["echo"]=="hello");
  assert(progress.size()==1&&progress[0]["status"]=="progress");
  assert(progress[0]["progress"]==1&&progress[0]["progressMessage"]=="halfway");
  auto slowA=std::async(std::launch::async,[&]{return client->callGadget("slow",
    {{"text","A"},{"delay",0.20}});});
  auto slowB=std::async(std::launch::async,[&]{return client->callGadget("slow",
    {{"text","B"},{"delay",0.05}});});
  assert(slowB.get().content[0]["text"]=="B");
  assert(slowA.get().content[0]["text"]=="A");
  bool gadgetError=false;
  try{(void)client->callGadget("error",batbox::Json::object());}
  catch(const batbox::mcp::GadgetCallError& error){gadgetError=std::string(error.what())=="tool failure"&&
    error.meta["reason"]=="fixture";}
  assert(gadgetError);
  std::stop_source stop;
  auto pending=std::async(std::launch::async,[&]{
    try{(void)client->callGadget("slow",{{"text","cancelled"},{"delay",0.4}},stop.get_token());
      return false;}
    catch(const batbox::ConnectorAbortError&){return true;}});
  std::this_thread::sleep_for(std::chrono::milliseconds(50));stop.request_stop();
  assert(pending.get());
  assert(client->callGadget("echo",{{"text","after cancel"}}).content[0]["text"]=="after cancel");
  std::cout<<"native MCP stdio, progress, parallel calls, errors and cancellation passed\n";
}

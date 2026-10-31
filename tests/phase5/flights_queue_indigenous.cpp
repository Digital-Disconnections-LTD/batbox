#include "flights/WriteQueue.hpp"
#include "flights/Load.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
using namespace batbox;
int main(){
  namespace s=batbox::flights;
  auto base=std::filesystem::temp_directory_path()/("batbox-queue-fixture-"+std::to_string(getpid()));
  std::filesystem::remove_all(base);const auto file=(base/"nested/session.jsonl").string();
  {
    s::WriteQueue queue;
    auto written=queue.enqueueWrite(file,Json{{"type","user"},{"uuid","u"}});
    assert(written.wait_for(std::chrono::seconds(3))==std::future_status::ready);written.get();
    queue.flush();
  }
  struct stat mode{};assert(::stat(file.c_str(),&mode)==0 && (mode.st_mode&0777)==0600);
  assert(::stat((base/"nested").c_str(),&mode)==0 && (mode.st_mode&0777)==0700);
  std::ifstream input(file);std::string data((std::istreambuf_iterator<char>(input)),{});
  assert(s::parseJSONLBuffer(data).size()==1);
  {
    // A synchronized producer can update an entry before delayed serialization.
    std::mutex mutex;Json value={{"type","assistant"},{"usage",0}};
    s::WriteQueue queue(s::indigenousQueueServices(),std::chrono::seconds(10));
    const auto done=queue.enqueueWrite(file,s::WriteQueue::EntryReader([&]{std::lock_guard lock(mutex);return value;}));
    {std::lock_guard lock(mutex);value["usage"]=123;}
    queue.flush();done.get();
  }
  std::ifstream resumed(file);data.assign(std::istreambuf_iterator<char>(resumed),{});
  assert(s::parseJSONLBuffer(data).back().at("usage")==123);
  {
    // New files appended while I/O is active are visible to the Map-order drain;
    // a new entry on the file already visited waits for the following drain.
    Json trace=Json::array();s::WriteQueue* pointer=nullptr;bool first=true;
    s::QueueServices services;
    services.appendFile=[&](const std::string& path,const std::string&){
      trace.push_back(path);
      if(first){first=false;(void)pointer->enqueueWrite("a",Json{{"second",true}});(void)pointer->enqueueWrite("c",Json{{"third",true}});}
    };
    services.mkdir=[](const std::string&){};
    s::WriteQueue queue(services,std::chrono::seconds(10));pointer=&queue;
    (void)queue.enqueueWrite("a",Json{{"first",true}});(void)queue.enqueueWrite("b",Json{{"first",true}});
    queue.flush();assert(trace==Json::array({"a","b","c"}));
    queue.flush();assert(trace==Json::array({"a","b","c","a"}));
  }
  {
    s::QueueServices services;services.appendFile=[](const auto&,const auto&){throw std::runtime_error("disk error");};services.mkdir=[](const auto&){};
    s::WriteQueue queue(services,std::chrono::seconds(10));const auto done=queue.enqueueWrite("file",Json{{"x",1}});
    bool failed=false;try{queue.flush();}catch(const std::runtime_error& e){failed=std::string(e.what())=="disk error";}assert(failed);
    assert(done.wait_for(std::chrono::seconds(0))==std::future_status::timeout);
  }
  std::filesystem::remove_all(base);
}

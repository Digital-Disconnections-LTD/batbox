#include "flights/async_queue/Queue.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
using namespace batbox;
using namespace batbox::event_loop;
using namespace batbox::flights::async_queue;
namespace {
struct Fixture {
  Loop loop;
  std::vector<std::string> trace;
  std::vector<Completion> writes,mkdirs;
  Queue queue;
  explicit Fixture(std::size_t maxChunk=1000):queue(loop,imports(),1,maxChunk){}
  QueueImports imports(){
    QueueImports result;
    result.appendFile=[this](const std::string& file,const std::string& data){
      trace.push_back("append:"+file+":"+value::stringify(data));
      Completion pending(loop);writes.push_back(pending);return pending;};
    result.mkdir=[this](const std::string& directory){
      trace.push_back("mkdir:"+directory);
      Completion pending(loop);mkdirs.push_back(pending);return pending;};
    result.onBackgroundError=[](std::exception_ptr error){std::rethrow_exception(error);};
    return result;
  }
  void enqueue(const std::string& file,int id){
    auto pending=queue.enqueueWrite(file,Json{{"id",id}});
    pending.then([this,id](const Unit*,const std::exception_ptr& error){
      trace.push_back("done:"+std::to_string(id)+(error?":error":""));});
  }
  void observed(Completion pending,const std::string& label){
    pending.then([this,label](const Unit*,const std::exception_ptr& error){
      trace.push_back(label+(error?":error":""));});
  }
  void tick(){loop.runReady();}
  void fire(){loop.run();}
};
Json asJson(const std::vector<std::string>& trace){Json value=Json::array();
  for(const auto& item:trace)value.push_back(item);
  return value;}
std::vector<std::string> coalescedFlush(){Fixture s;
  s.enqueue("a",1);s.enqueue("a",2);s.observed(s.queue.flush(),"flush");
  s.tick();s.writes.at(0).resolve({});s.tick();return s.trace;}
std::vector<std::string> overlap(){Fixture s;
  s.enqueue("a",1);s.fire();s.enqueue("b",2);s.fire();
  s.writes.at(1).resolve({});s.tick();s.writes.at(0).resolve({});s.tick();return s.trace;}
std::vector<std::string> addedDuringDrain(){Fixture s;
  s.enqueue("a",1);s.fire();s.enqueue("a",2);s.writes.at(0).resolve({});s.tick();
  s.fire();s.writes.at(1).resolve({});s.tick();return s.trace;}
std::vector<std::string> retryAfterMissingDirectory(){Fixture s;
  s.enqueue("nested/a",1);s.observed(s.queue.flush(),"flush");s.tick();
  s.writes.at(0).reject(std::make_exception_ptr(std::runtime_error("missing")));s.tick();
  s.mkdirs.at(0).resolve({});s.tick();s.writes.at(1).resolve({});s.tick();return s.trace;}
std::vector<std::string> tracked(){Fixture s;
  Deferred<int> operation(s.loop);
  auto pending=s.queue.trackWrite<int>([operation]{return operation;});
  pending.then([&s](const int*,const std::exception_ptr& error){
    s.trace.push_back(error?"tracked:error":"tracked");});
  s.observed(s.queue.flush(),"flush");s.tick();operation.resolve(7);s.tick();return s.trace;}
std::vector<std::string> chunkThreshold(){Fixture s(1);
  s.enqueue("a",1);s.observed(s.queue.flush(),"flush");s.tick();
  s.writes.at(0).resolve({});s.tick();s.writes.at(1).resolve({});s.tick();return s.trace;}
std::vector<std::string> newFileDuringDrain(){Fixture s;
  s.enqueue("a",1);s.fire();s.enqueue("b",2);s.writes.at(0).resolve({});s.tick();
  s.writes.at(1).resolve({});s.tick();s.fire();s.tick();return s.trace;}
std::vector<std::string> persistentFailure(){Fixture s;
  s.enqueue("nested/a",1);s.observed(s.queue.flush(),"flush");s.tick();
  s.writes.at(0).reject(std::make_exception_ptr(std::runtime_error("missing")));s.tick();
  s.mkdirs.at(0).resolve({});s.tick();
  s.writes.at(1).reject(std::make_exception_ptr(std::runtime_error("denied")));s.tick();
  return s.trace;}
}
int main(){Json result=Json::object();
  result["coalescedFlush"]=asJson(coalescedFlush());
  result["overlap"]=asJson(overlap());
  result["addedDuringDrain"]=asJson(addedDuringDrain());
  result["retryAfterMissingDirectory"]=asJson(retryAfterMissingDirectory());
  result["tracked"]=asJson(tracked());
  result["chunkThreshold"]=asJson(chunkThreshold());
  result["newFileDuringDrain"]=asJson(newFileDuringDrain());
  result["persistentFailure"]=asJson(persistentFailure());
  std::cout<<value::stringify(result)<<'\n';}

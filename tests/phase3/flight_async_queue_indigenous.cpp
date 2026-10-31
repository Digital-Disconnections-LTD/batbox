#include <cstdlib>
#include "flights/async_queue/Queue.hpp"
#include "event_loop/AsyncContext.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
using namespace batbox;
using namespace batbox::event_loop;
using namespace batbox::flights::async_queue;
namespace {
std::string temporaryDirectory(){
  std::string pattern="/tmp/batbox-queue-XXXXXX";
  std::vector<char> chars(pattern.begin(),pattern.end());chars.push_back('\0');
  char* created=::mkdtemp(chars.data());
  if(!created)throw std::runtime_error("mkdtemp failed");
  return created;
}
std::string readAll(const std::string& file){
  std::ifstream input(file,std::ios::binary);
  return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
void realFilesAndKeepalive(){
  const auto root=temporaryDirectory();
  try{
    const auto file=root+"/nested/deep/session.jsonl";
    Loop loop;bool first=false,second=false;int errors=0;
    Queue queue(loop,indigenousQueueImports(loop,[&](std::exception_ptr){++errors;}));
    queue.enqueueWrite(file,Json{{"id",1},{"message","☃"}}).then(
      [&](const Unit*,const std::exception_ptr& error){assert(!error);first=true;});
    queue.enqueueWrite(file,Json{{"id",2}}).then(
      [&](const Unit*,const std::exception_ptr& error){assert(!error);second=true;});
    bool flushed=false;
    queue.flush().then([&](const Unit*,const std::exception_ptr& error){
      assert(!error);flushed=true;});
    // A referenced KeepAlive holds run() across every worker completion.
    loop.run();
    assert(first&&second&&flushed&&errors==0);
    assert(readAll(file)=="{\"id\":1,\"message\":\"☃\"}\n{\"id\":2}\n");
    struct stat fileInfo{},directoryInfo{};
    assert(::stat(file.c_str(),&fileInfo)==0);
    assert(::stat((root+"/nested/deep").c_str(),&directoryInfo)==0);
    assert((fileInfo.st_mode&077)==0);
    assert((directoryInfo.st_mode&077)==0);
    // The standard 100 ms coalescing timer still writes without flush().
    bool third=false;
    queue.enqueueWrite(file,Json{{"id",3}}).then(
      [&](const Unit*,const std::exception_ptr& error){assert(!error);third=true;});
    loop.run();
    assert(third&&errors==0);
    assert(readAll(file)=="{\"id\":1,\"message\":\"☃\"}\n{\"id\":2}\n{\"id\":3}\n");
  }catch(...){std::filesystem::remove_all(root);throw;}
  std::filesystem::remove_all(root);
}
void permanentFailure(){
  const auto root=temporaryDirectory();
  try{
    const auto blocked=root+"/blocked";
    {std::ofstream output(blocked);output<<"file";}
    Loop loop;int errors=0;bool failed=false;
    Queue queue(loop,indigenousQueueImports(loop,[&](std::exception_ptr){++errors;}));
    queue.enqueueWrite(blocked+"/child/session.jsonl",Json{{"id",1}});
    queue.flush().then([&](const Unit*,const std::exception_ptr& error){failed=bool(error);});
    loop.run();
    assert(failed&&errors==0);
  }catch(...){std::filesystem::remove_all(root);throw;}
  std::filesystem::remove_all(root);
}
void pooledWorkContextAndErrors(){
  Loop loop;
  auto key=std::make_shared<runtime::AsyncContextKey>();
  auto value=std::make_shared<int>(42);
  const auto owner=std::this_thread::get_id();
  int completed=0,failures=0;
  {
    runtime::AsyncContextScope scope(runtime::extendAsyncContext(key,value));
    for(int i=0;i<32;++i)loop.queueWork([key,value,i,owner]{
      assert(std::this_thread::get_id()!=owner);
      assert(runtime::currentAsyncContextValue(key)==value);
      if(i%7==0)throw std::runtime_error("pooled worker failure");
    },[&loop,key,value,&completed,&failures](std::exception_ptr error){
      assert(loop.isOwnerThread());
      assert(runtime::currentAsyncContextValue(key)==value);
      ++completed;if(error)++failures;
    });
  }
  // Work requests retain the loop without a timer or uv_async keepalive each.
  loop.run();
  assert(completed==32&&failures==5);
  assert(!runtime::currentAsyncContextValue(key));
}
}
int main(){realFilesAndKeepalive();permanentFailure();pooledWorkContextAndErrors();}

#include "cli/process_output/ProcessOutput.hpp"
#include "fs_operations/NodeFsAsyncIndigenous.hpp"
#include <cerrno>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>

using namespace batbox;
int main(){
  ::alarm(10);
  event_loop::Loop loop;
  auto workers=std::make_shared<fs_operations::IndigenousAsyncFsHost>(loop,1);
  int out[2],err[2];
  if(::pipe(out)||::pipe(err))throw std::runtime_error("pipe failed");
  cli::process_output::ProcessOutput output(loop,workers,out[1],err[1]);
  output.registerProcessOutputErrorHandlers();
  const std::string payload(256*1024,'x');
  output.writeToStdout(payload);
  std::string received;
  std::thread consumer([&]{
    char buffer[8192];
    while(received.size()<payload.size()){
      const auto count=::read(out[0],buffer,sizeof buffer);
      if(count<0&&errno==EINTR)continue;
      if(count<=0)break;
      received.append(buffer,static_cast<std::size_t>(count));
    }
  });
  loop.run();consumer.join();
  if(received!=payload)throw std::runtime_error("Buffered output mismatch");
  ::close(out[0]);
  output.writeToStdout("broken");
  loop.run();
  if(!output.stdoutDestroyed())throw std::runtime_error("EPIPE not handled");
  output.writeToStdout("ignored");
  ::close(out[1]);::close(err[0]);::close(err[1]);
  ::alarm(0);
}

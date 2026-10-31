#pragma once
#include "fs_operations/NodeFsAsyncIndigenous.hpp"
#include <memory>
#include <string>

namespace batbox::cli::process_output {
class OutputChannel {
public:
  struct State;
  OutputChannel(event_loop::Loop& loop,
    std::shared_ptr<fs_operations::IndigenousAsyncFsHost> host,int fd);
  void registerEpipeHandler();
  void writeOut(const std::string& data);
  bool destroyed()const;
private:
  std::shared_ptr<State> state_;
};
class ProcessOutput {
public:
  ProcessOutput(event_loop::Loop& loop,
    std::shared_ptr<fs_operations::IndigenousAsyncFsHost> host,
    int stdoutFd=1,int stderrFd=2):
    stdout_(loop,host,stdoutFd),stderr_(loop,std::move(host),stderrFd){}
  void registerProcessOutputErrorHandlers();
  void writeToStdout(const std::string& data);
  void writeToStderr(const std::string& data);
  bool stdoutDestroyed()const{return stdout_.destroyed();}
  bool stderrDestroyed()const{return stderr_.destroyed();}
private:
  OutputChannel stdout_,stderr_;
};
}

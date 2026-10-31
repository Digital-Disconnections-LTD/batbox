#pragma once
#include "shell/ManagedShell.hpp"
#ifndef _WIN32
#include <sys/types.h>
#endif
namespace batbox::shell {
#ifdef _WIN32
using ShellProcessId=unsigned long;
#else
using ShellProcessId=pid_t;
#endif
struct ManagedShell::Job {
  std::string missionId,command,description,outputPath,cwdPath,startingCwd;
  ShellProcessId pid=0;
  std::mutex mutex;
  std::condition_variable cv;
  bool completed=false,postExitReady=false,callbackDone=false,
    background=false,backgroundedByUser=false,killed=false,timedOut=false,
    killedForSize=false;
  bool cancelledBySignal=false;
  std::exception_ptr completionFailure;
  std::unique_ptr<std::stop_callback<std::function<void()>>> signalCallback;
  int code=0;
  double timeoutMs=0;
  std::chrono::steady_clock::time_point started;
};
std::string quotePosix(const std::string& value);
std::string readOutputPrefix(const std::string& path,std::size_t maxBytes,std::size_t& totalBytes);
std::string utf8WithReplacement(std::string_view bytes);
void terminateGroup(ShellProcessId pid);
} // namespace batbox::shell

#include "cleanup/Exit.hpp"
#include <cstdlib>
#include <csignal>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace batbox::cleanup {
std::function<void(int)> indigenousProcessExit(){
  return [](int code){std::_Exit(code);};
}
std::function<void()> indigenousKillSelf(){
  return []{
#ifdef _WIN32
    ::TerminateProcess(::GetCurrentProcess(),137);
#else
    ::kill(::getpid(),SIGKILL);
#endif
  };
}
} // namespace batbox::cleanup

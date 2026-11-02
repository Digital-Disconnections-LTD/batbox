#include "application/ErrorReport.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace batbox;
using namespace batbox::application;
namespace {
int failures=0;
void check(bool condition,const std::string& message){
  if(!condition){++failures;std::cerr<<"FAIL: "<<message<<'\n';}
}
bool contains(const std::string& text,const std::string& part){
  return text.find(part)!=std::string::npos;
}
template<class F>std::exception_ptr captured(F&& thrower){
  try{thrower();}catch(...){return std::current_exception();}
  return {};
}
std::string pastTheEnd(){
  const std::string branch="main";
  return branch.substr(branch.size()+1);
}
}

int main(){
  // The status row once read "Error: basic_string": std::string::substr past
  // the end throws std::out_of_range whose what() is that bare token on libc++.
  {
    const auto error=captured([]{(void)pastTheEnd();});
    check(static_cast<bool>(error),"substr past the end throws");
    const auto described=describeError(error);
    check(described.internal,"out_of_range is an internal error");
    const auto status=statusErrorText(described,"prompt history");
    check(status=="Error: internal error in prompt history · details in native.log",
      "readable status for out_of_range, got: "+status);
    check(!contains(status,"basic_string")&&!contains(status,"out_of_range"),
      "status hides the library token: "+status);
    const auto log=errorLogLine(described,"prompt history");
    check(contains(log,"prompt history")&&contains(log,"out_of_range")&&
      contains(log,described.what),"log keeps origin, type and what(): "+log);
  }
  // A command UI flattens exceptions into runtime_error(what()).
  {
    const auto described=describeError(std::make_exception_ptr(
      std::runtime_error("basic_string")));
    check(described.internal,"bare library token is internal");
    check(statusErrorText(described,"commands")==
      "Error: internal error in commands · details in native.log","flattened token hidden");
    check(contains(errorLogLine(described,"commands"),"basic_string"),"flattened token logged");
  }
  // Messages written for people still reach the status row, on one line.
  {
    const auto described=describeError(std::make_exception_ptr(std::runtime_error(
      "Set VISUAL or EDITOR to edit Bat configurations")));
    check(!described.internal,"user-facing runtime_error stays visible");
    check(statusErrorText(described,"commands")==
      "Error: Set VISUAL or EDITOR to edit Bat configurations","user message unchanged");
    const auto multi=describeError(std::make_exception_ptr(std::runtime_error(
      "Hook failed:\n\x1b[31mexit code 2\x1b[0m\ttry again\r\n")));
    const auto status=statusErrorText(multi,"SessionStart hooks");
    check(status=="Error: Hook failed: exit code 2 try again",
      "status is one line without control characters, got: "+status);
  }
  {
    const auto json=captured([]{Json value=Json::array();(void)value.at("missing");});
    check(describeError(json).internal,"JSON exceptions are internal");
    const auto length=captured([]{std::string text;text.reserve(text.max_size()+1);});
    check(describeError(length).internal,"length_error is internal");
    const auto unknown=captured([]{throw 42;});
    check(describeError(unknown).internal&&
      contains(statusErrorText(describeError(unknown),"Gadgets"),"internal error in Gadgets"),
      "non-standard exceptions are internal");
  }
  check(statusErrorText(describeError({}),"anything").empty(),"no error, no status");
  check(statusLine("  a\n\nb\x7f c  ")=="a b c","statusLine collapses whitespace");
  check(statusLine("\x1b]8;;https://example.com\x1b\\link\x1b]8;;\x07 done")=="link done",
    "statusLine drops OSC sequences, got: "+statusLine("\x1b]8;;https://example.com\x1b\\link\x1b]8;;\x07 done"));
  if(failures){std::cerr<<failures<<" failure(s)\n";return 1;}
  std::cout<<"error report: ok\n";
  return 0;
}

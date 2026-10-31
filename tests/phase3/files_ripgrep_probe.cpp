#include "filesystem/RipgrepProcess.hpp"
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <iostream>
#include <thread>
using batbox::Json;
static Json evaluate(const Json& t){
  batbox::filesystem::RipgrepConfig config;
  config.command=t.value("command",std::string("rg"));
  config.timeoutMs=t.value("timeoutMs",20000);
  config.maxBufferBytes=t.value("maxBufferBytes",static_cast<std::size_t>(20000000));
  std::vector<std::string> args;
  if(t.contains("args"))for(const auto& x:t.at("args"))args.push_back(x.get<std::string>());
  std::stop_source source;
  std::jthread stopper;
  if(t.value("stop",false))stopper=std::jthread([&](){
    std::this_thread::sleep_for(std::chrono::milliseconds(20));source.request_stop();});
  return batbox::filesystem::ripGrep(args,t.at("target").get<std::string>(),source.get_token(),config);
}
int main(){std::string input((std::istreambuf_iterator<char>(std::cin)),{});
  try{std::cout<<batbox::value::stringify(Json{{"result",evaluate(batbox::value::parse(input))}})<<'\n';}
  catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}}

#pragma once
#include "poetryEntries/AsyncSystemPoetry.hpp"
#include "poetryEntries/PoetryBlocks.hpp"

namespace batbox::poetryEntries::async_poetry {
struct Run : std::enable_shared_from_this<Run> {
  event_loop::Loop::Dispatcher owner;
  AsyncSystemPoetryInput input;
  AsyncSystemPoetryServices services;
  event_loop::Deferred<std::vector<std::string>> result;
  PoetryObject commands,style,settings;
  GadgetNames enabled;
  std::vector<std::string> output;
  std::vector<std::string> sectionNames;
  Run(event_loop::Loop& loop,AsyncSystemPoetryInput in,AsyncSystemPoetryServices svc)
    :owner(loop.dispatcher()),input(std::move(in)),services(std::move(svc)),result(loop){}
  void start();
  void importsReady();
  void dynamic();
  void finish(const std::vector<SectionValue>&);
};
template<class Fn> void need(const Fn& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("Missing system prompt binding: ")+name);
}
inline constexpr const char* summarize=
  "When working with tool results, write down any important information you "
  "might need later in your response, as the original tool result may be cleared later.";
}

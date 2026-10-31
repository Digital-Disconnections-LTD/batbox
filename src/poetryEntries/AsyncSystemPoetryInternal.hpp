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
  std::string environment;
  GadgetNames enabled;
  std::vector<std::string> output;
  Run(event_loop::Loop& loop,AsyncSystemPoetryInput in,AsyncSystemPoetryServices svc)
    :owner(loop.dispatcher()),input(std::move(in)),services(std::move(svc)),result(loop){}
  void start();
  void importsReady();
  void proactive();
  void proactiveAfterMemory(const SectionValue&);
  void dynamic();
  void finish(const std::vector<SectionValue>&);
};
template<class Fn> void need(const Fn& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("Missing system prompt binding: ")+name);
}
inline void append(std::vector<std::string>& values,const SectionValue& value){
  if(value)values.push_back(*value);
}
// Fixed poetry shared by the asynchronous stages. The synchronous
// compatibility entry point assembles its result separately.
inline constexpr const char* summarize=
  "When working with tool results, write down any important information you "
  "might need later in your response, as the original tool result may be cleared later.";
inline constexpr const char* numericLength=
  "Length limits: keep text between tool calls to ≤25 words. Keep final "
  "responses to ≤100 words unless the task requires more detail.";
inline constexpr const char* tokenBudget=
  "When the user specifies a token target (e.g., \"+500k\", \"spend 2M tokens\", "
  "\"use 1B tokens\"), your output token count will be shown each turn. "
  "Keep working until you approach the target — plan your work to fill it "
  "productively. The target is a hard minimum, not a suggestion. If you stop "
  "early, the system will automatically continue you.";
}

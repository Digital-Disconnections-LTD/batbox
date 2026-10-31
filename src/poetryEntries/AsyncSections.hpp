#pragma once
#include "poetryEntries/Sections.hpp"
#include "event_loop/Deferred.hpp"
#include <variant>

namespace batbox::poetryEntries {
using AsyncSectionResult=std::variant<SectionValue,event_loop::Deferred<SectionValue>,
  event_loop::Deferred<std::string>>;
using ComputeAsyncSection=std::function<AsyncSectionResult()>;
struct AsyncPoetrySection {
  std::string name;
  ComputeAsyncSection compute;
  bool stashBreak=false;
};
AsyncPoetrySection systemPoetrySection(std::string name,ComputeAsyncSection compute);
AsyncPoetrySection uncachedSystemPoetrySection(std::string name,
  ComputeAsyncSection compute,const std::string& reason);
class AsyncSectionStash {
  struct State;
  std::shared_ptr<State> state_;
public:
  explicit AsyncSectionStash(event_loop::Loop& loop);
  event_loop::Deferred<std::vector<SectionValue>> resolveSystemPoetrySections(
    const std::vector<AsyncPoetrySection>& sections);
  void clearSystemPoetrySections(const std::function<void()>& clearBetaHeaderLatches);
  std::optional<SectionValue> stashed(const std::string& name)const;
};
}

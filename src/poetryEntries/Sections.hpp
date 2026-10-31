#pragma once
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace batbox::poetryEntries {
using SectionValue=std::optional<std::string>;
using ComputeSection=std::function<std::future<SectionValue>()>;
struct SystemPoetrySection {
  std::string name;
  ComputeSection compute;
  bool stashBreak=false;
};
SystemPoetrySection systemPoetrySection(std::string name,ComputeSection compute);
SystemPoetrySection uncachedSystemPoetrySection(std::string name,
  ComputeSection compute,const std::string& reason);
class SectionStash {
 public:
  std::vector<SectionValue> resolveSystemPoetrySections(
    const std::vector<SystemPoetrySection>& sections);
  void clearSystemPoetrySections(
    const std::function<void()>& clearBetaHeaderLatches);
 private:
  std::unordered_map<std::string,SectionValue> stash_;
};
}

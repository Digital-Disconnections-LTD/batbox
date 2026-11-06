#pragma once
#include "poetryEntries/Sections.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::poetryEntries {
inline constexpr const char* kSystemPoetryDynamicBoundary=
  "__SYSTEM_PROMPT_DYNAMIC_BOUNDARY__";
struct GadgetNames {
  std::vector<std::string> ordered;
  bool contains(const std::string& name) const{
    return std::find(ordered.begin(),ordered.end(),name)!=ordered.end();
  }
  void insert(const std::string& name){
    if(!contains(name))ordered.push_back(name);
  }
  auto begin() const{return ordered.begin();}
  auto end() const{return ordered.end();}
};
struct SystemPoetryInputs {
  Json environment=Json::object();
  std::function<std::string()> getCwd,getFlightStartDate;
  std::function<bool()> globalStashScope;
  // Prepared static head: the built-in poetry/system.md or the user's
  // system.md, with {{section:NAME}} placeholders still in place.
  std::function<std::string()> systemHead;
  std::function<std::string(const GadgetNames&)> usingYourGadgetsSection;
  std::function<std::optional<std::string>(const GadgetNames&,const Json&)>
    flightSpecificGuidanceSection;
};
}

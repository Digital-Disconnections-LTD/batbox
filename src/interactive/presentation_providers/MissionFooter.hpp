#pragma once
#include "runtime/JsonValue.hpp"
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/color.hpp>
#include <string>

namespace batbox::interactive::presentation_providers {
// A compact, display-ready reading of one Bat mission record. The footer and
// the Bats list share it so both surfaces describe a Bat the same way.
struct BatGlance {
  std::string label,status,elapsed,gadget,lastText,model;
  long long gadgetUses=0,tokens=0;
  bool foreground=false,running=false;
};
BatGlance batGlance(const Json& mission,long long nowMs);
std::string compactDuration(long long milliseconds);
std::string compactTokens(long long tokens);
Json backgroundFooterMissions(const Json& missions);
std::size_t visibleFooterMissions(const Json& missions,int terminalRows);
ftxui::Element missionFooter(const Json& missions,int columns,int terminalRows,
  ftxui::Color color);
ftxui::Element missionFooterAt(const Json& missions,int columns,int terminalRows,
  ftxui::Color color,long long nowMs);
}

#pragma once
#include "runtime/JsonValue.hpp"
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/color.hpp>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

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
// Active Missions rows in display order: the Bat tree first, each sub-Bat
// under its parent with a "depth", then shells and other background work.
// The open Bat and its parents stay listed after they settle.
Json backgroundFooterMissions(const Json& missions,
  const std::optional<std::string>& current=std::nullopt);
// The Bat rows of backgroundFooterMissions, top to bottom.
std::vector<std::string> footerBatIds(const Json& rows);
// Gadget uses that launched the running Bats among these rows.
std::unordered_set<std::string> runningBatGadgetUses(const Json& rows);
// The Bat that launched `id`, when it is still known.
std::optional<std::string> parentBatId(const Json& missions,const std::string& id);
// Bat titles from the top-level Bat down to `id`.
std::vector<std::string> batTrail(const Json& rows,const std::string& id);
// The rows the footer shows: at most five, scrolled to keep `current` in
// view, or none when collapsed or the terminal is too short for them.
struct FooterWindow {
  bool collapsed=false;
  std::size_t begin=0,end=0;
};
// extraRows: rows the list shows above the missions (the Flight row while
// the list has focus).
FooterWindow footerWindow(const Json& rows,int terminalRows,bool collapsed,
  const std::optional<std::string>& current,int extraRows=0);
struct FooterOptions {
  int columns=80,terminalRows=24;
  ftxui::Color color=ftxui::Color::Default;
  long long nowMs=0;
  std::optional<std::string> current;
  // Set while the list has focus: the focused row's id, empty for the
  // Flight row, which then leads the list.
  std::optional<std::string> selected;
  bool collapsed=false;
};
ftxui::Element missionFooter(const Json& rows,FooterOptions options);
ftxui::Element missionFooterAt(const Json& rows,const FooterOptions& options);
}

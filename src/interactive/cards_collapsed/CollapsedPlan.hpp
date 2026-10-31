#pragma once

#include "interactive/MessageLookups.hpp"
#include "gadgets/Gadget.hpp"
#include <functional>
#include <optional>

namespace batbox::interactive::cards_collapsed {

struct Span {
  std::string text;
  bool bold=false;
  std::optional<std::string> hyperlink;
};
struct CollapsedCard {
  bool visible=false,verbose=false,active=false,error=false;
  Json selectedBackground=Json();
  Json sourceMessage=Json();
  Json theme=Json();
  std::vector<Span> summary;
  std::vector<std::string> hintLines;
  std::optional<std::string> hookSummary;
  std::vector<Json> verboseGadgetUses;
  std::vector<Json> relevantMemories;
  Json hookInfos=Json();
  std::string expandHint;
  bool shouldAnimate=false;
};
struct CollapsedCounts {
  double read=0,search=0,list=0,mcp=0,bash=0;
};
struct CollapsedState {
  CollapsedCounts maximum;
};
struct CollapsedServices {
  std::function<Json()> readSelectedBackground;
  std::function<Json()> readTheme;
  std::function<bool()> isFullscreen;
  std::function<bool()> teamMemoryEnabled;
  std::function<std::string(const std::string&)> displayPath;
  // Minimum display time remains an owner-loop hook dependency.
  std::function<std::optional<std::string>(const std::optional<std::string>&,int)>
      minimumDisplayTime;
  std::function<std::string(double)> formatDuration;
  std::function<std::string(double)> formatSecondsShort;
  std::function<std::string()> expandHint;
  std::function<std::string(const std::string&)> stripMcpServerPrefix;
};
struct CollapsedInputs {
  Json message;
  const GadgetIDs* inProgressGadgetUseIDs=nullptr;
  bool shouldAnimate=false,verbose=false,active=false;
  const gadgets::Gadgets* gadgets=nullptr;
  const MessageLookups* lookups=nullptr;
};

CollapsedCard classifyCollapsedReadSearch(CollapsedState& state,
   const CollapsedInputs& input,const CollapsedServices& services);

// Display order: Git outcomes, read/search/list/REPL/MCP/shell, then memory.
void appendCollapsedSummary(std::vector<Span>& out,const Json& message,
   const CollapsedCounts& counts,bool fullscreen,bool active,
   bool teamMemoryEnabled,const CollapsedServices& services);
}

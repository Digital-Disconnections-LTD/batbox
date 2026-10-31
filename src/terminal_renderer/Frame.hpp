#pragma once
#include "terminal_screen/Screen.hpp"
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace batbox::terminal_renderer {
using terminal_screen::ScreenRef;
using terminal_screen::Size;
struct Cursor {double x=0,y=0;bool visible=true;};
struct ScrollHint {double top=0,bottom=0,delta=0;};
struct Frame {
  ScreenRef screen;
  Size viewport;
  Cursor cursor;
  // Outer optional distinguishes an omitted hint from explicit null.
  std::optional<std::optional<ScrollHint>> scrollHint;
  std::optional<bool> scrollDrainPending;
};
Frame emptyFrame(double rows,double columns,
  std::shared_ptr<terminal_screen::StylePool> styles,
  std::shared_ptr<terminal_screen::CharPool> chars,
  std::shared_ptr<terminal_screen::HyperlinkPool> links,
  const terminal_screen::DebugWarning& debug);

enum class FlickerReason {Resize,Offscreen,Clear};
std::optional<FlickerReason> shouldClearScreen(const Frame& previous,
  const Frame& frame);
struct FramePhases {
  double renderer=0,diff=0,optimize=0,write=0,patches=0,yoga=0,
    commit=0,yogaVisited=0,yogaMeasured=0,yogaStashHits=0,yogaLive=0;
};
struct Flicker {double desiredHeight=0,availableHeight=0;FlickerReason reason;};
struct FrameEvent {
  double durationMs=0;
  std::optional<FramePhases> phases;
  std::vector<Flicker> flickers;
};
struct StdoutPatch {std::string content;};
struct ClearPatch {double count=0;};
struct ClearTerminalPatch {
  FlickerReason reason;
  struct Debug {double triggerY=0;std::string prevLine,nextLine;};
  std::optional<Debug> debug;
};
struct CursorHidePatch {};
struct CursorShowPatch {};
struct CursorMovePatch {double x=0,y=0;};
struct CursorToPatch {double col=0;};
struct CarriageReturnPatch {};
struct HyperlinkPatch {std::string uri;};
struct StyleStringPatch {std::string str;};
using Patch=std::variant<StdoutPatch,ClearPatch,ClearTerminalPatch,
  CursorHidePatch,CursorShowPatch,CursorMovePatch,CursorToPatch,
  CarriageReturnPatch,HyperlinkPatch,StyleStringPatch>;
using Diff=std::vector<Patch>;
} // namespace batbox::terminal_renderer

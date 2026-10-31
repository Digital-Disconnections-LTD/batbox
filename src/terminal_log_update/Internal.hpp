#pragma once
#include "terminal_log_update/LogUpdate.hpp"
#include "terminal_screen/Geometry.hpp"
#include <optional>

namespace batbox::terminal_log_update {
using terminal_renderer::Diff;
using terminal_renderer::Frame;
using terminal_renderer::FlickerReason;
using terminal_screen::Cell;
using terminal_screen::StylePool;
struct Delta {double dx=0,dy=0;};
Diff crlfPatches(double rows);
class VirtualScreen {
public:
  terminal_screen::Point cursor;
  Diff diff;
  const double viewportWidth;
  VirtualScreen(terminal_screen::Point origin,double viewportWidth):
    cursor(origin),viewportWidth(viewportWidth){}
  void txn(const std::function<std::pair<Diff,Delta>(
    const terminal_screen::Point&)>& fn);
};
std::optional<std::string> transitionHyperlink(Diff& diff,
  const std::optional<std::string>& current,
  const std::optional<std::string>& target);
std::int32_t transitionStyle(Diff& diff,StylePool& styles,
  std::int32_t currentId,std::int32_t targetId);
std::string readLine(const terminal_screen::Screen& screen,double y);
Diff fullResetSequence(const Frame& frame,FlickerReason reason,
  StylePool& styles,std::optional<terminal_renderer::ClearTerminalPatch::Debug>
    debug={});
void renderFrame(VirtualScreen& screen,const Frame& frame,StylePool& styles);
VirtualScreen& renderFrameSlice(VirtualScreen& screen,const Frame& frame,
  double startY,double endY,StylePool& styles);
bool writeCellWithStyleStr(VirtualScreen& screen,const Cell& cell,
  const std::string& styleStr);
void moveCursorTo(VirtualScreen& screen,double targetX,double targetY);
bool needsWidthCompensation(const std::string& character);
std::string trimEndJs(const std::string& text);
std::string fixedOne(double value);
} // namespace batbox::terminal_log_update

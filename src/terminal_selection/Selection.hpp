#pragma once
#include "terminal_screen/Screen.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::terminal_selection {
struct Point {double col=0,row=0;};
using PointRef=std::shared_ptr<Point>;
enum class SpanKind {Word,Line};
enum class FocusMove {Left,Right,Up,Down,LineStart,LineEnd};
struct AnchorSpan {PointRef lo,hi;SpanKind kind=SpanKind::Word;};
using SpanRef=std::shared_ptr<AnchorSpan>;
struct SelectionState {
  PointRef anchor,focus;
  bool isDragging=false;
  SpanRef anchorSpan;
  std::shared_ptr<std::vector<std::string>> scrolledOffAbove=
    std::make_shared<std::vector<std::string>>();
  std::shared_ptr<std::vector<std::string>> scrolledOffBelow=
    std::make_shared<std::vector<std::string>>();
  std::shared_ptr<std::vector<bool>> scrolledOffAboveSW=
    std::make_shared<std::vector<bool>>();
  std::shared_ptr<std::vector<bool>> scrolledOffBelowSW=
    std::make_shared<std::vector<bool>>();
  std::optional<double> virtualAnchorRow,virtualFocusRow;
  bool lastPressHadAlt=false;
};
struct Bounds {PointRef start,end;};

SelectionState createSelectionState();
void startSelection(SelectionState& state,double col,double row);
void updateSelection(SelectionState& state,double col,double row);
void finishSelection(SelectionState& state);
void clearSelection(SelectionState& state);
void selectWordAt(SelectionState& state,const terminal_screen::Screen& screen,
  double col,double row);
std::optional<std::string> findPlainTextUrlAt(
  const terminal_screen::Screen& screen,double col,double row);
void selectLineAt(SelectionState& state,const terminal_screen::Screen& screen,
  double row);
void extendSelection(SelectionState& state,
  const terminal_screen::Screen& screen,double col,double row);
void moveFocus(SelectionState& state,double col,double row);
void shiftSelection(SelectionState& state,double dRow,double minRow,
  double maxRow,double width);
void shiftAnchor(SelectionState& state,double dRow,double minRow,double maxRow);
bool shiftSelectionForFollow(SelectionState& state,double dRow,
  double minRow,double maxRow);
bool hasSelection(const SelectionState& state);
std::optional<Bounds> selectionBounds(const SelectionState& state);
bool isCellSelected(const SelectionState& state,double col,double row);
std::string getSelectedText(const SelectionState& state,
  const terminal_screen::Screen& screen);
void captureScrolledRows(SelectionState& state,
  const terminal_screen::Screen& screen,double firstRow,double lastRow,
  bool above);
void applySelectionOverlay(terminal_screen::Screen& screen,
  const SelectionState& selection,terminal_screen::StylePool& stylePool);

int comparePoints(const Point& a,const Point& b);
std::optional<std::pair<double,double>> wordBoundsAt(
  const terminal_screen::Screen& screen,double col,double row);
std::string extractRowText(const terminal_screen::Screen& screen,double row,
  double colStart,double colEnd);
void joinRows(std::vector<std::string>& lines,const std::string& text,
  std::optional<bool> softWrap);
} // namespace batbox::terminal_selection

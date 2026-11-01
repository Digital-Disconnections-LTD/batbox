#pragma once
#include "terminal_ftxui/PresentationView.hpp"
#include <ftxui/dom/node.hpp>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace batbox::terminal_ftxui {
struct CopyPosition {
  std::size_t row=0;
  int column=0;
};

// A frozen, plain-text transcript with selection coordinates independent of
// the visible terminal viewport. Mouse drags can extend while the view scrolls.
class TranscriptCopy {
public:
  TranscriptCopy(const interactive::PresentationState& state,int columns,int rows);
  void resize(const interactive::PresentationState& state,int columns,int rows);
  void scrollBy(int rows);
  void press(int x,int y);
  void move(int x,int y);
  std::optional<std::string> release(int x,int y);
  std::string selectedText()const;
  int edgeDirection()const;
  bool dragging()const{return dragging_;}
  std::size_t top()const{return top_;}
  ftxui::Element render()const;
private:
  CopyPosition position(int x,int y)const;
  std::pair<CopyPosition,CopyPosition> ordered()const;
  int bodyHeight()const;
  void clampTop();
  std::vector<VisualLine> lines_;
  int columns_=80,rows_=24,mouseX_=0,mouseY_=1;
  std::size_t top_=0;
  std::optional<CopyPosition> anchor_,end_;
  bool dragging_=false,moved_=false;
};
}

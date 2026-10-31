#pragma once
#include "terminal_screen/Screen.hpp"
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>

namespace batbox::terminal_output {
using terminal_screen::ScreenRef;
using terminal_screen::StylePool;
using terminal_screen::Rectangle;
using terminal_screen::DebugWarning;
struct ClusteredChar {
  std::string value;
  double width=0;
  std::int32_t styleId=0;
  std::optional<std::string> hyperlink;
};
using ClusteredCharRef=std::shared_ptr<ClusteredChar>;
using ClusteredChars=std::vector<ClusteredCharRef>;
using ClusteredCharsRef=std::shared_ptr<ClusteredChars>;
using CharStash=std::unordered_map<std::string,ClusteredCharsRef>;
struct Clip {
  std::optional<double> x1,x2,y1,y2;
};
using ClipRef=std::shared_ptr<Clip>;
ClipRef intersectClip(const ClipRef& parent,const ClipRef& child);
std::optional<double> maxDefined(std::optional<double> a,std::optional<double> b);
std::optional<double> minDefined(std::optional<double> a,std::optional<double> b);
struct WriteOperation {
  double x=0,y=0;
  std::string text;
  std::shared_ptr<std::vector<bool>> softWrap;
};
struct ClipOperation {ClipRef clip;};
struct UnclipOperation {};
struct BlitOperation {
  ScreenRef src;
  double x=0,y=0,width=0,height=0;
};
struct ShiftOperation {double top=0,bottom=0,n=0;};
struct ClearOperation {
  std::shared_ptr<Rectangle> region;
  std::optional<bool> fromAbsolute;
};
struct NoSelectOperation {std::shared_ptr<Rectangle> region;};
using Operation=std::variant<WriteOperation,ClipOperation,UnclipOperation,
  BlitOperation,ShiftOperation,ClearOperation,NoSelectOperation>;
struct OutputRenderServices;
class Output {
public:
  Output(double width,double height,std::shared_ptr<StylePool> stylePool,
    ScreenRef screen,DebugWarning warning);
  void reset(double width,double height,ScreenRef screen);
  void blit(ScreenRef src,double x,double y,double width,double height);
  void shift(double top,double bottom,double n);
  void clear(std::shared_ptr<Rectangle> region,
    std::optional<bool> fromAbsolute={});
  void noSelect(std::shared_ptr<Rectangle> region);
  void write(double x,double y,std::string text,
    std::shared_ptr<std::vector<bool>> softWrap={});
  void clip(ClipRef value);
  void unclip();
  ScreenRef get(const OutputRenderServices& services);
  double width=0,height=0;
  std::shared_ptr<StylePool> stylePool;
  ScreenRef screen;
  std::vector<Operation> operations;
  CharStash charStash;
private:
  DebugWarning warning_;
};
} // namespace batbox::terminal_output

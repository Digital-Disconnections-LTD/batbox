#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace batbox::termio::parser {
struct Color {
  std::string type="default",name;
  double index=0,r=0,g=0,b=0;
};
struct TextStyle {
  bool bold=false,dim=false,italic=false,blink=false,inverse=false;
  bool hidden=false,strikethrough=false,overline=false;
  std::string underline="none";
  Color fg,bg,underlineColor;
};
struct Grapheme {std::string value;int width=1;};
struct TextAction {std::vector<Grapheme> graphemes;TextStyle style;};
struct CursorAction {
  std::string type,direction,style;
  double count=1,row=1,col=1;
  bool blinking=false;
};
struct EraseAction {std::string type,region;double count=1;};
struct ScrollAction {std::string type;double count=1,top=1,bottom=1;};
struct ModeAction {std::string type,mode;bool enabled=false;};
struct LinkAction {std::string type,url;std::optional<Json> params;};
struct TitleAction {std::string type,value;};
struct TabStatusAction {
  std::optional<std::optional<Color>> indicator,statusColor;
  std::optional<std::optional<std::string>> status;
};
struct SgrAction {std::string params;};
struct BellAction {};
struct ResetAction {};
struct UnknownAction {std::string sequence;};
using Action=std::variant<TextAction,CursorAction,EraseAction,ScrollAction,
  ModeAction,LinkAction,TitleAction,TabStatusAction,SgrAction,BellAction,
  ResetAction,UnknownAction>;

TextStyle defaultStyle();
Json toJson(const Color& color);
Json toJson(const TextStyle& style);
Json toJson(const Action& action);
Json toJson(const std::vector<Action>& actions);
}

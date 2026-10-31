#include "terminal_optimizer/Optimizer.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace tr=batbox::terminal_renderer;
namespace to=batbox::terminal_optimizer;
namespace {
tr::Patch parsePatch(const Json& value){
  const auto type=value.at("type").get<std::string>();
  if(type=="stdout")return tr::StdoutPatch{value.at("content")};
  if(type=="clear")return tr::ClearPatch{value.at("count")};
  if(type=="cursorMove")return tr::CursorMovePatch{
    value.at("x"),value.at("y")};
  if(type=="cursorTo")return tr::CursorToPatch{value.at("col")};
  if(type=="cursorHide")return tr::CursorHidePatch{};
  if(type=="cursorShow")return tr::CursorShowPatch{};
  if(type=="carriageReturn")return tr::CarriageReturnPatch{};
  if(type=="styleStr")return tr::StyleStringPatch{value.at("str")};
  if(type=="hyperlink")return tr::HyperlinkPatch{value.at("uri")};
  if(type=="clearTerminal")return tr::ClearTerminalPatch{
    value.at("reason")=="resize"?tr::FlickerReason::Resize:
    value.at("reason")=="offscreen"?tr::FlickerReason::Offscreen:
    tr::FlickerReason::Clear,{}};
  throw std::invalid_argument("Unknown patch type");
}
Json jsonPatch(const tr::Patch& patch){
  if(const auto p=std::get_if<tr::StdoutPatch>(&patch))
    return Json{{"type","stdout"},{"content",p->content}};
  if(const auto p=std::get_if<tr::ClearPatch>(&patch))
    return Json{{"type","clear"},{"count",p->count}};
  if(const auto p=std::get_if<tr::CursorMovePatch>(&patch))
    return Json{{"type","cursorMove"},{"x",p->x},{"y",p->y}};
  if(const auto p=std::get_if<tr::CursorToPatch>(&patch))
    return Json{{"type","cursorTo"},{"col",p->col}};
  if(std::holds_alternative<tr::CursorHidePatch>(patch))
    return Json{{"type","cursorHide"}};
  if(std::holds_alternative<tr::CursorShowPatch>(patch))
    return Json{{"type","cursorShow"}};
  if(std::holds_alternative<tr::CarriageReturnPatch>(patch))
    return Json{{"type","carriageReturn"}};
  if(const auto p=std::get_if<tr::StyleStringPatch>(&patch))
    return Json{{"type","styleStr"},{"str",p->str}};
  if(const auto p=std::get_if<tr::HyperlinkPatch>(&patch))
    return Json{{"type","hyperlink"},{"uri",p->uri}};
  const auto& p=std::get<tr::ClearTerminalPatch>(patch);
  return Json{{"type","clearTerminal"},{"reason",
    p.reason==tr::FlickerReason::Resize?"resize":
    p.reason==tr::FlickerReason::Offscreen?"offscreen":"clear"}};
}
Json jsonDiff(const tr::Diff& patches){
  Json result=Json::array();
  for(const auto& patch:patches)result.push_back(jsonPatch(patch));
  return result;
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("case path required");
  std::ifstream file(argv[1]);
  const Json cases=Json::parse(file);
  Json rows=Json::array();
  for(const auto& item:cases){
    auto input=std::make_shared<tr::Diff>();
    for(const auto& value:item.at("patches"))input->push_back(parsePatch(value));
    const Json original=jsonDiff(*input);
    const auto result=to::optimize(input);
    rows.push_back(Json{{"name",item.at("name")},
      {"same",result==input},{"patches",jsonDiff(*result)},
      {"original",original}});
  }
  std::cout<<rows.dump()<<'\n';
}

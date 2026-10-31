#include "terminal_renderer/Frame.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <iterator>
#include <limits>

using namespace batbox;
namespace batbox::terminal_screen {
StylePool::StylePool(){}
CharPool::CharPool(){}
HyperlinkPool::HyperlinkPool(){}
ScreenRef createScreen(double width,double height,
    const std::shared_ptr<StylePool>&,
    const std::shared_ptr<CharPool>&,
    const std::shared_ptr<HyperlinkPool>&,
    const DebugWarning&){
  auto result=std::make_shared<Screen>();
  result->width=width;result->height=height;return result;
}
}
namespace {
double number(const Json& value){
  return value.is_string()?
    std::numeric_limits<double>::quiet_NaN():value.get<double>();
}
Json run(const Json& spec){
  terminal_renderer::Frame previous,next;
  previous.screen=std::make_shared<terminal_screen::Screen>();
  next.screen=std::make_shared<terminal_screen::Screen>();
  previous.screen->height=number(spec.at("previousHeight"));
  previous.viewport={number(spec.at("previousWidth")),
    number(spec.at("previousRows"))};
  next.screen->height=number(spec.at("height"));
  next.viewport={number(spec.at("width")),number(spec.at("rows"))};
  const auto reason=terminal_renderer::shouldClearScreen(previous,next);
  std::string label;
  if(reason)label=*reason==terminal_renderer::FlickerReason::Resize?
    "resize":"offscreen";
  auto empty=terminal_renderer::emptyFrame(number(spec.at("rows")),
    number(spec.at("width")),
    std::make_shared<terminal_screen::StylePool>(),
    std::make_shared<terminal_screen::CharPool>(),
    std::make_shared<terminal_screen::HyperlinkPool>(),
    [](const std::string&,const std::string&){});
  return {{"reason",reason?Json(label):Json(nullptr)},
    {"empty",{{"screen",Json::array({empty.screen->width,
        empty.screen->height})},
      {"viewport",{{"width",empty.viewport.width},
        {"height",empty.viewport.height}}},
      {"cursor",{{"x",empty.cursor.x},{"y",empty.cursor.y},
        {"visible",empty.cursor.visible}}}}}};
}
}
int main(){
  const std::string input(std::istreambuf_iterator<char>(std::cin),{});
  const auto cases=value::parse(input);
  Json output=Json::array();
  for(const auto& item:cases)output.push_back(run(item));
  std::cout<<value::stringify(output)<<'\n';
}

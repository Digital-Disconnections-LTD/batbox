#include "terminal_output/Output.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <iostream>

using namespace batbox;
namespace output=batbox::terminal_output;
namespace screen=batbox::terminal_screen;
namespace {
struct Pools {
  std::shared_ptr<screen::CharPool> chars=std::make_shared<screen::CharPool>();
  std::shared_ptr<screen::HyperlinkPool> links=std::make_shared<screen::HyperlinkPool>();
  std::shared_ptr<screen::StylePool> styles=std::make_shared<screen::StylePool>();
  screen::ScreenRef make(int width,int height){
    return screen::createScreen(width,height,styles,chars,links,{});
  }
};
Json clip(const output::Clip& value){
  auto field=[](std::optional<double> n){return n?Json(*n):Json(nullptr);};
  return Json{{"x1",field(value.x1)},{"x2",field(value.x2)},
    {"y1",field(value.y1)},{"y2",field(value.y2)}};
}
std::string type(const output::Operation& value){
  if(std::holds_alternative<output::WriteOperation>(value))return "write";
  if(std::holds_alternative<output::ClipOperation>(value))return "clip";
  if(std::holds_alternative<output::UnclipOperation>(value))return "unclip";
  if(std::holds_alternative<output::BlitOperation>(value))return "blit";
  if(std::holds_alternative<output::ShiftOperation>(value))return "shift";
  if(std::holds_alternative<output::ClearOperation>(value))return "clear";
  return "noSelect";
}
}
int main(){
  Json rows=Json::array();
  {
    auto child=std::make_shared<output::Clip>(output::Clip{1,5,{},8});
    auto parent=std::make_shared<output::Clip>(output::Clip{0,4,2,10});
    auto intersection=output::intersectClip(parent,child);
    rows.push_back(Json{{"kind","clip"},
      {"sameUnbounded",output::intersectClip({},child)==child},
      {"intersect",clip(*intersection)},
      {"maxUndefined",*output::maxDefined({},3)},
      {"minUndefined",*output::minDefined(4,{})},
      {"maxNegativeZero",std::signbit(*output::maxDefined(-0.0,0.0))},
      {"minNegativeZero",std::signbit(*output::minDefined(-0.0,0.0))},
      {"maxNaN",std::isnan(*output::maxDefined(
        std::numeric_limits<double>::quiet_NaN(),1))}});
  }
  {
    Pools p;auto s=p.make(2,1);s->cells->setWord(0,2);
    output::Output o(2,1,p.styles,s,{});
    auto region=std::make_shared<screen::Rectangle>(screen::Rectangle{0,0,1,1});
    auto c=std::make_shared<output::Clip>(output::Clip{0,2,0,1});
    auto wrap=std::make_shared<std::vector<bool>>(
      std::initializer_list<bool>{false,true});
    o.write(0,0,"");o.clear(region,false);o.noSelect(region);
    o.clip(c);o.write(1,0,"A",wrap);o.unclip();o.blit(s,0,0,1,1);
    o.shift(0,1,1);region->width=2;c->x2=1;(*wrap)[1]=false;
    Json types=Json::array();
    for(const auto& value:o.operations)types.push_back(type(value));
    const auto& first=std::get<output::ClearOperation>(o.operations[0]);
    const auto& second=std::get<output::NoSelectOperation>(o.operations[1]);
    const auto& third=std::get<output::ClipOperation>(o.operations[2]);
    const auto& fourth=std::get<output::WriteOperation>(o.operations[3]);
    const auto& sixth=std::get<output::BlitOperation>(o.operations[5]);
    rows.push_back(Json{{"kind","queue"},
      {"resetFirst",s->cells->word(0)==0},{"types",types},
      {"regionSame",first.region==region&&second.region==region},
      {"regionWidth",first.region->width},{"clipSame",third.clip==c},
      {"clipX2",*third.clip->x2},{"wrapSame",fourth.softWrap==wrap},
      {"wrapValue",(*fourth.softWrap)[1]},
      {"falsePresent",first.fromAbsolute.has_value()},
      {"blitScreenSame",sixth.src==s}});
    const auto* operations=&o.operations;
    const auto* stash=&o.charStash;
    o.charStash.emplace("A",std::make_shared<output::ClusteredChars>(
      output::ClusteredChars{std::make_shared<output::ClusteredChar>(
        output::ClusteredChar{"A",1,0,{}})}));
    auto next=p.make(3,1);o.reset(3,1,next);
    rows.push_back(Json{{"kind","resetKeepCache"},
      {"sameOperations",&o.operations==operations},
      {"opLength",o.operations.size()},{"sameCache",&o.charStash==stash},
      {"cacheSize",o.charStash.size()},{"screenSame",o.screen==next},
      {"width",o.width},{"height",o.height}});
    for(int i=0;i<16385;i++)
      o.charStash.emplace("key"+std::to_string(i),
        std::make_shared<output::ClusteredChars>());
    o.reset(3,1,next);
    rows.push_back(Json{{"kind","resetCap"},
      {"sameCache",&o.charStash==stash},{"cacheSize",o.charStash.size()}});
  }
  std::cout<<rows.dump()<<'\n';
}

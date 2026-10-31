#include "terminal_renderer/Renderer.hpp"
#include "indigenous_style_trace.hpp"
#include <cmath>
#include <iostream>
#include <iterator>
#include <map>

using namespace batbox;
namespace {
Json* activeTrace=nullptr;
std::map<terminal_output::Output*,int> outputIds;
int nextOutputId=0;
}
namespace batbox::terminal_screen {
StylePool::StylePool(){}
CharPool::CharPool(){}
HyperlinkPool::HyperlinkPool(){}
ScreenRef createScreen(double width,double height,
    const std::shared_ptr<StylePool>&,
    const std::shared_ptr<CharPool>& chars,
    const std::shared_ptr<HyperlinkPool>& links,
    const DebugWarning&){
  activeTrace->push_back(Json::array({"createScreen",width,height}));
  auto screen=std::make_shared<Screen>();
  screen->width=width;screen->height=height;
  screen->charPool=chars;screen->hyperlinkPool=links;
  return screen;
}
}
namespace batbox::terminal_output {
Output::Output(double w,double h,std::shared_ptr<StylePool> styles,
    ScreenRef target,DebugWarning warning):width(w),height(h),
    stylePool(std::move(styles)),screen(std::move(target)),
    warning_(std::move(warning)){
  const int id=++nextOutputId;outputIds[this]=id;
  screen->width=w;screen->height=h;
  activeTrace->push_back(Json::array({"new",id,w,h}));
}
void Output::reset(double w,double h,ScreenRef target){
  width=w;height=h;screen=std::move(target);
  screen->width=w;screen->height=h;
  activeTrace->push_back(Json::array({"reset",outputIds[this],w,h}));
}
ScreenRef Output::get(const OutputRenderServices&){
  activeTrace->push_back(Json::array({"get",outputIds[this]}));
  return screen;
}
}
namespace batbox::terminal_dom {
void markDirty(DOMNode* node){
  static_cast<DOMElement*>(node)->dirty=true;
  activeTrace->push_back(Json::array({"dirty"}));
}
}
namespace {
class LayoutProbe:public terminal_dom::style_test::TraceLayoutNode {
public:
  double width=0,height=0;
  double getComputedWidth()const override{return width;}
  double getComputedHeight()const override{return height;}
};
double dimension(const Json& value){
  if(value.is_string()){
    const auto s=value.get<std::string>();
    if(s=="nan")return std::numeric_limits<double>::quiet_NaN();
    if(s=="inf")return std::numeric_limits<double>::infinity();
  }
  return value.get<double>();
}
Json run(const Json& spec){
  Json trace=Json::array(),frames=Json::array();
  activeTrace=&trace;outputIds.clear();nextOutputId=0;
  auto node=std::make_shared<terminal_dom::DOMElement>("ink-root");
  for(int i=0;i<spec.value("children",0);++i)
    node->childNodes.push_back(std::make_shared<terminal_dom::TextNode>(""));
  if(!spec.contains("layout")||spec.at("layout")!=false){
    auto layout=std::make_shared<LayoutProbe>();
    layout->width=dimension(spec.at("width"));
    layout->height=dimension(spec.at("height"));
    node->yogaNode=layout;
  }
  auto drain=std::make_shared<terminal_dom::DOMElement>("ink-box");
  auto styles=std::make_shared<terminal_screen::StylePool>();
  auto chars=std::make_shared<terminal_screen::CharPool>();
  auto links=std::make_shared<terminal_screen::HyperlinkPool>();
  terminal_dom::NodeStash stash;
  if(spec.value("absoluteRemoved",false))
    stash.addPendingClear(*node,
      std::make_shared<terminal_dom::StashedLayout>(),true);
  terminal_screen::ScreenRef currentFront;
  terminal_renderer::RendererServices services;
  services.resetLayoutShifted=[&]{trace.push_back(Json::array({"resetLayoutShifted"}));};
  services.resetScrollHint=[&]{trace.push_back(Json::array({"resetScrollHint"}));};
  services.resetScrollDrainNode=[&]{trace.push_back(Json::array({"resetScrollDrainNode"}));};
  services.renderNodeToOutput=[&](terminal_dom::DOMElement&,
      terminal_output::Output& output,terminal_screen::ScreenRef previous){
    trace.push_back(Json::array({"render",outputIds[&output],
      previous&&previous==currentFront?Json("front"):Json(nullptr)}));
  };
  services.getScrollDrainNode=[&]()->terminal_dom::DOMElement*{
    trace.push_back(Json::array({"getScrollDrainNode"}));
    return spec.value("drain",false)?drain.get():nullptr;
  };
  services.getScrollHint=[&]()->std::optional<terminal_renderer::ScrollHint>{
    trace.push_back(Json::array({"getScrollHint"}));
    if(!spec.contains("hint")||spec.at("hint").is_null())return {};
    const auto& h=spec.at("hint");
    return terminal_renderer::ScrollHint{h.at("top").get<double>(),
      h.at("bottom").get<double>(),h.at("delta").get<double>()};
  };
  services.logForDebugging=[&](const std::string& message,
      const std::string& level){
    trace.push_back(Json::array({"log",message,level}));
  };
  services.outputRender.line.tokenize=[](const std::string&)
      ->ansi_tokenize::Tokens{throw std::logic_error("fake Output.get only");};
  services.outputRender.line.styledCharsFromTokens=[](
      const ansi_tokenize::Tokens&)->ansi_tokenize::StyledChars{
    throw std::logic_error("fake Output.get only");};
  services.outputRender.line.getGraphemeSegmenter=[]()
      ->std::shared_ptr<intl::Segmenter>{
    throw std::logic_error("fake Output.get only");};
  services.outputRender.line.stringWidth=[](const std::string&)->double{
    throw std::logic_error("fake Output.get only");};
  services.outputRender.line.reorderBidi=[](
      const terminal_output::ClusteredCharsRef&)
      ->terminal_output::ClusteredCharsRef{
    throw std::logic_error("fake Output.get only");};
  services.outputRender.widestLine=[](const std::string&)->double{
    throw std::logic_error("fake Output.get only");};
  services.outputRender.sliceAnsi=[](const std::string&,double,double)
      ->std::string{throw std::logic_error("fake Output.get only");};
  services.outputRender.logForDebugging=[](const std::string&){
    throw std::logic_error("fake Output.get only");};
  auto renderer=terminal_renderer::createRenderer(*node,styles,stash,services);
  const Json defaultPass=Json::object();
  const auto passes=spec.contains("passes")?spec.at("passes"):
    Json::array({defaultPass});
  for(const auto& pass:passes){
    auto front=std::make_shared<terminal_screen::Screen>();
    front->width=1;front->height=1;
    auto back=std::make_shared<terminal_screen::Screen>();
    back->width=2;back->height=2;
    back->charPool=chars;back->hyperlinkPool=links;
    currentFront=front;
    terminal_renderer::RenderOptions options;
    options.frontFrame.screen=front;options.backFrame.screen=back;
    options.isTTY=pass.value("isTTY",spec.value("isTTY",true));
    options.terminalWidth=pass.value("terminalWidth",
      spec.value("terminalWidth",20.0));
    options.terminalRows=pass.value("terminalRows",
      spec.value("terminalRows",6.0));
    options.altScreen=pass.value("altScreen",spec.value("altScreen",false));
    options.prevFrameContaminated=pass.value("contaminated",false);
    const auto frame=renderer->render(options);
    Json result={{"screen",Json::array({frame.screen->width,frame.screen->height})},
      {"screenIsBack",frame.screen==back},
      {"viewport",{{"width",frame.viewport.width},{"height",frame.viewport.height}}},
      {"cursor",{{"x",frame.cursor.x},{"y",frame.cursor.y},
        {"visible",frame.cursor.visible}}},
      {"scrollHint","absent"},
      {"scrollDrainPending","absent"},
      {"drainDirty",drain->dirty}};
    if(frame.scrollHint){
      if(*frame.scrollHint){
        const auto& hint=**frame.scrollHint;
        result["scrollHint"]={{"top",hint.top},{"bottom",hint.bottom},
          {"delta",hint.delta}};
      }else result["scrollHint"]=nullptr;
    }
    if(frame.scrollDrainPending)
      result["scrollDrainPending"]=*frame.scrollDrainPending;
    frames.push_back(std::move(result));
  }
  return {{"frames",frames},{"trace",trace}};
}
}
int main(){
  const std::string input(std::istreambuf_iterator<char>(std::cin),{});
  const auto cases=value::parse(input);
  Json output=Json::array();
  for(const auto& spec:cases)output.push_back(run(spec));
  std::cout<<value::stringify(output)<<'\n';
}

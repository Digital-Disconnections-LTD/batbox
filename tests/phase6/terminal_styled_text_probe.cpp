#include "terminal_render/StyledText.hpp"
#include "terminal_dom/FullLayoutPort.hpp"
#include <iostream>
#include <algorithm>

using namespace batbox;
namespace rend=batbox::terminal_render;
namespace dom=batbox::terminal_dom;
namespace {
struct FakeLayout:dom::FullLayoutNode {
  double left=2,top=1;
  void insertChild(dom::LayoutNode&,std::size_t)override{}
  void removeChild(dom::LayoutNode&)override{}
  std::size_t getChildCount()const override{return 0;}
  void setMeasureFunc(dom::MeasureFunction)override{}
  void markDirty()override{}
  dom::LayoutDisplay getDisplay()const override{return dom::LayoutDisplay::Flex;}
  double getComputedTop()const override{return top;}
  double getComputedHeight()const override{return 0;}
  dom::FullLayoutRef getParent()const override{return {};}
  void calculateLayout(std::optional<double>,std::optional<double>)override{}
  void unsetMeasureFunc()override{}
  double getComputedLeft()const override{return left;}
  double getComputedWidth()const override{return 0;}
  double getComputedBorder(const std::string&)const override{return 0;}
  double getComputedPadding(const std::string&)const override{return 0;}
#define SET_DOUBLE(name) void name(double)override{}
#define SET_STRING(name) void name(const std::string&)override{}
#define SET_STRING_DOUBLE(name) void name(const std::string&,double)override{}
  SET_DOUBLE(setWidth) SET_DOUBLE(setWidthPercent)
  void setWidthAuto()override{}
  SET_DOUBLE(setHeight) SET_DOUBLE(setHeightPercent)
  void setHeightAuto()override{}
  SET_DOUBLE(setMinWidth) SET_DOUBLE(setMinWidthPercent)
  SET_DOUBLE(setMinHeight) SET_DOUBLE(setMinHeightPercent)
  SET_DOUBLE(setMaxWidth) SET_DOUBLE(setMaxWidthPercent)
  SET_DOUBLE(setMaxHeight) SET_DOUBLE(setMaxHeightPercent)
  SET_STRING(setFlexDirection) SET_DOUBLE(setFlexGrow)
  SET_DOUBLE(setFlexShrink) SET_DOUBLE(setFlexBasis)
  SET_DOUBLE(setFlexBasisPercent) SET_STRING(setFlexWrap)
  SET_STRING(setAlignItems) SET_STRING(setAlignSelf)
  SET_STRING(setJustifyContent)
  void setDisplay(dom::LayoutDisplay)override{}
  SET_STRING(setPositionType) SET_STRING_DOUBLE(setPosition)
  SET_STRING_DOUBLE(setPositionPercent) SET_STRING(setOverflow)
  SET_STRING_DOUBLE(setMargin) SET_STRING_DOUBLE(setPadding)
  SET_STRING_DOUBLE(setBorder) SET_STRING_DOUBLE(setGap)
  void free()override{}
  void freeRecursive()override{}
#undef SET_DOUBLE
#undef SET_STRING
#undef SET_STRING_DOUBLE
};
std::shared_ptr<dom::DOMElement> node(const std::string& name){
  return std::make_shared<dom::DOMElement>(name);
}
std::shared_ptr<dom::TextNode> text(const std::string& value){
  return std::make_shared<dom::TextNode>(value);
}
rend::StylesRef style(const char* tag){
  return std::make_shared<Json>(Json{{"tag",tag}});
}
rend::StyledTextImports imports(Json& calls){
  rend::StyledTextImports s;
  s.applyTextStyles=[&calls](const std::string& value,
      const rend::StylesRef& styles){
    const auto tag=styles&&styles->contains("tag")?
      (*styles)["tag"].get<std::string>():std::string{};
    calls.push_back(Json::array({value,tag}));
    return "["+tag+":"+value+"]";
  };
  s.wrapText=[](const std::string& value,double,const std::string& type){
    if(type=="wrap"||type=="wrap-trim"){
      std::string out;
      for(std::size_t i=0;i<value.size();i+=2){
        if(i)out+='\n';out+=value.substr(i,2);
      }
      return out;
    }
    auto out=value;
    std::transform(out.begin(),out.end(),out.begin(),[](unsigned char c){
      return static_cast<char>(std::toupper(c));});
    return out;
  };
  s.indentString=[](const std::string& value,double count){
    std::string out;std::size_t start=0;
    while(true){
      const auto end=value.find('\n',start);
      const auto line=value.substr(start,end==std::string::npos?end:end-start);
      if(!line.empty())out+=std::string(static_cast<std::size_t>(count),' ')+line;
      if(end==std::string::npos)break;
      out+='\n';start=end+1;
    }
    return out;
  };
  return s;
}
}
int main(){
  Json rows=Json::array(),calls=Json::array();
  const auto services=imports(calls);
  {
    auto link=node("ink-link");link->attributes["href"]="https://a";
    link->childNodes.push_back(text("b"));
    auto nested=node("ink-text");nested->childNodes.push_back(text("c"));
    link->childNodes.push_back(nested);
    auto child=node("ink-text");child->textStyles=style("inner");
    child->childNodes={text("a"),link,text("")};
    auto root=node("ink-text");root->textStyles=style("outer");
    auto ignored=node("ink-box");ignored->childNodes.push_back(text("x"));
    root->childNodes={child,text("d"),ignored};
    const auto values=rend::squashTextNodesToSegments(*root);
    Json segments=Json::array();
    for(const auto& value:*values)segments.push_back(Json{
      {"text",value.text},{"styles",*value.styles},
      {"hyperlink",value.hyperlink?Json(*value.hyperlink):Json(nullptr)}});
    rows.push_back(Json{{"kind","squash"},{"segments",segments},
      {"sameStyles",(*values)[0].styles==(*values)[1].styles},
      {"inheritedSame",(*values)[2].styles==root->textStyles}});
  }
  {
    rend::StyledSegments segments{{"A",style("a"),{}},
      {"😀",style("b"),{}},{"B",style("c"),{}}};
    rows.push_back(Json{{"kind","charMap"},
      {"map",rend::buildCharToSegmentMap(segments)}});
  }
  {
    rend::StyledSegments segments{{"AB   ",style("x"),{}},
      {"\tD",style("y"),"https://a"}};
    const auto map=rend::buildCharToSegmentMap(segments);
    calls=Json::array();
    const auto value=rend::applyStylesToWrappedText("AB\n\tD",
      segments,map,"AB   \tD",true,services);
    rows.push_back(Json{{"kind","trimStyles"},{"value",value},
      {"calls",calls}});
  }
  {
    rend::StyledSegments segments{{"A😀",style("x"),{}},
      {"B",style("y"),{}}};
    const auto map=rend::buildCharToSegmentMap(segments);
    calls=Json::array();
    const auto value=rend::applyStylesToWrappedText("A😀\nB",
      segments,map,"A😀B",false,services);
    rows.push_back(Json{{"kind","surrogateStyles"},{"value",value},
      {"calls",calls}});
  }
  for(const auto& type:{"wrap","wrap-trim","truncate"}){
    const auto value=rend::wrapWithSoftWrap("abcd\nef",2,type,services);
    rows.push_back(Json{{"kind","softWrap"},{"type",type},
      {"wrapped",value.wrapped},
      {"softWrap",value.softWrap?Json(*value.softWrap):Json(nullptr)}});
  }
  {
    auto parent=node("ink-text"),child=node("ink-box");
    child->yogaNode=std::make_shared<FakeLayout>();
    parent->childNodes.push_back(child);
    auto soft=std::make_shared<std::vector<bool>>(
      std::initializer_list<bool>{false,true});
    const auto value=rend::applyPaddingToText(*parent,"abc",soft,services);
    rows.push_back(Json{{"kind","padding"},{"value",value},
      {"softWrap",*soft}});
  }
  std::cout<<rows.dump()<<'\n';
}

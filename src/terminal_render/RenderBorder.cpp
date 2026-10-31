#include "terminal_render/Border.hpp"
#include "terminal_dom/FullLayoutPort.hpp"
#include "runtime/JsonValue.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_render {
namespace {
const Json* field(const Json& object,const char* name){
  if(!object.is_object())return nullptr;
  const auto found=object.find(name);
  return found==object.end()?nullptr:&*found;
}
const Json* nullish(const Json& object,const char* first,
    const char* second){
  const auto primary=field(object,first);
  return primary&&!primary->is_null()&&!primary->is_discarded()?
    primary:field(object,second);
}
std::optional<std::string> optionalString(const Json* value){
  if(!value||value->is_null()||value->is_discarded())return {};
  if(value->is_string())return value->get<std::string>();
  throw std::invalid_argument("Border string value required");
}
std::string requiredString(const Json& object,const char* name){
  const auto value=field(object,name);
  if(!value||!value->is_string())
    throw std::invalid_argument(std::string("Border field required: ")+name);
  return value->get<std::string>();
}
bool shown(const Json& style,const char* name){
  const auto value=field(style,name);
  return !(value&&value->is_boolean()&&!value->get<bool>());
}
std::string repeat(const std::string& value,double count){
  const auto n=std::isnan(count)?0:std::trunc(count);
  if(!std::isfinite(n)||n<0)
    throw std::range_error("Invalid count value");
  std::string result;
  for(std::size_t i=0;i<static_cast<std::size_t>(n);i++)result+=value;
  return result;
}
std::string substring(const std::string& value,double start,double end){
  const auto units=value::utf16CodeUnits(value);
  const auto len=static_cast<double>(units.size());
  auto bound=[&](double position){
    if(std::isnan(position)||position<=0)return std::size_t{0};
    if(position>=len)return units.size();
    return static_cast<std::size_t>(std::trunc(position));
  };
  auto a=bound(start),b=bound(end);
  if(a>b)std::swap(a,b);
  return value::fromUtf16(units.substr(a,b-a));
}
BoxStyle customBox(const Json& object){
  return BoxStyle{requiredString(object,"topLeft"),
    requiredString(object,"top"),requiredString(object,"topRight"),
    requiredString(object,"right"),requiredString(object,"bottomRight"),
    requiredString(object,"bottom"),requiredString(object,"bottomLeft"),
    requiredString(object,"left")};
}
std::optional<BoxStyle> resolveBox(const Json& style,
    const BorderImports& imports){
  if(style.is_string()){
    if(!imports.resolveBox)
      throw std::invalid_argument("CLI boxes import required");
    return imports.resolveBox(style.get<std::string>());
  }
  if(style.is_object())return customBox(style);
  return {};
}
std::optional<std::string> color(const Json& style,const char* primary,
    const char* fallback){
  return optionalString(nullish(style,primary,fallback));
}
bool dimColor(const Json& style,const char* primary,
    const char* fallback){
  const auto value=nullish(style,primary,fallback);
  return value&&value::truthy(*value);
}
}
EmbeddedBorderText embedTextInBorder(const std::string& borderLine,
    const std::string& text,const std::string& align,double offset,
    const std::string& borderChar,const BorderImports& imports){
  if(!imports.stringWidth)
    throw std::invalid_argument("Border width import required");
  const auto textLength=imports.stringWidth(text);
  const auto borderLength=static_cast<double>(value::stringLength(borderLine));
  if(textLength>=borderLength-2)
    return {"",substring(text,0,borderLength),""};
  double position;
  if(align=="center")position=std::floor((borderLength-textLength)/2);
  else if(align=="start")position=offset+1;
  else position=borderLength-textLength-offset-1;
  position=terminal_screen::typed_array::jsMax(1,
    terminal_screen::typed_array::jsMin(position,borderLength-textLength-1));
  const auto before=substring(borderLine,0,1)+repeat(borderChar,position-1);
  const auto after=repeat(borderChar,borderLength-position-textLength-1)+
    substring(borderLine,borderLength-1,borderLength);
  return {before,text,after};
}
std::string styleBorderLine(const std::string& line,
    const std::optional<std::string>& colorValue,bool dim,
    const BorderImports& imports){
  if(!imports.applyColor)
    throw std::invalid_argument("Border applyColor import required");
  auto styled=imports.applyColor(line,colorValue);
  if(dim){
    if(!imports.dim)throw std::invalid_argument("Border dim import required");
    styled=imports.dim(styled);
  }
  return styled;
}
void renderBorder(double x,double y,const terminal_dom::DOMNode& node,
    terminal_output::Output& output,const BorderImports& imports){
  if(!node.style||!node.style->is_object())return;
  const auto& style=*node.style;
  const auto border=field(style,"borderStyle");
  if(!border||!value::truthy(*border))return;
  const auto yoga=std::dynamic_pointer_cast<terminal_dom::FullLayoutNode>(
    node.yogaNode);
  if(!yoga)throw std::invalid_argument("Border full layout node required");
  const auto width=std::floor(yoga->getComputedWidth());
  const auto height=std::floor(yoga->getComputedHeight());
  const auto box=resolveBox(*border,imports);
  if(!box)throw std::invalid_argument("Unknown border style");
  const auto topColor=color(style,"borderTopColor","borderColor");
  const auto bottomColor=color(style,"borderBottomColor","borderColor");
  const auto leftColor=color(style,"borderLeftColor","borderColor");
  const auto rightColor=color(style,"borderRightColor","borderColor");
  const bool topDim=dimColor(style,"borderTopDimColor","borderDimColor");
  const bool bottomDim=dimColor(style,"borderBottomDimColor","borderDimColor");
  const bool leftDim=dimColor(style,"borderLeftDimColor","borderDimColor");
  const bool rightDim=dimColor(style,"borderRightDimColor","borderDimColor");
  const bool showTop=shown(style,"borderTop");
  const bool showBottom=shown(style,"borderBottom");
  const bool showLeft=shown(style,"borderLeft");
  const bool showRight=shown(style,"borderRight");
  const auto contentWidth=terminal_screen::typed_array::jsMax(0,
    width-(showLeft?1:0)-(showRight?1:0));
  const auto topLine=showTop?(showLeft?box->topLeft:"")+
    repeat(box->top,contentWidth)+(showRight?box->topRight:""):"";
  const auto borderText=field(style,"borderText");
  std::optional<std::string> topBorder;
  if(showTop&&borderText&&borderText->is_object()&&
      optionalString(field(*borderText,"position"))=="top"){
    const auto text=requiredString(*borderText,"content");
    const auto align=requiredString(*borderText,"align");
    const auto offset=field(*borderText,"offset");
    const auto parts=embedTextInBorder(topLine,text,align,
      offset&&offset->is_number()?offset->get<double>():0,
      box->top,imports);
    topBorder=styleBorderLine(parts[0],topColor,topDim,imports)+parts[1]+
      styleBorderLine(parts[2],topColor,topDim,imports);
  }else if(showTop)topBorder=styleBorderLine(topLine,topColor,topDim,imports);
  double verticalHeight=height;
  if(showTop)verticalHeight-=1;
  if(showBottom)verticalHeight-=1;
  verticalHeight=terminal_screen::typed_array::jsMax(0,verticalHeight);
  if(!imports.applyColor)
    throw std::invalid_argument("Border applyColor import required");
  auto leftBorder=repeat(imports.applyColor(box->left,leftColor)+"\n",
    verticalHeight);
  if(leftDim){
    if(!imports.dim)throw std::invalid_argument("Border dim import required");
    leftBorder=imports.dim(leftBorder);
  }
  auto rightBorder=repeat(imports.applyColor(box->right,rightColor)+"\n",
    verticalHeight);
  if(rightDim){
    if(!imports.dim)throw std::invalid_argument("Border dim import required");
    rightBorder=imports.dim(rightBorder);
  }
  const auto bottomLine=showBottom?(showLeft?box->bottomLeft:"")+
    repeat(box->bottom,contentWidth)+(showRight?box->bottomRight:""):"";
  std::optional<std::string> bottomBorder;
  if(showBottom&&borderText&&borderText->is_object()&&
      optionalString(field(*borderText,"position"))=="bottom"){
    const auto text=requiredString(*borderText,"content");
    const auto align=requiredString(*borderText,"align");
    const auto offset=field(*borderText,"offset");
    const auto parts=embedTextInBorder(bottomLine,text,align,
      offset&&offset->is_number()?offset->get<double>():0,
      box->bottom,imports);
    bottomBorder=styleBorderLine(parts[0],bottomColor,bottomDim,imports)+
      parts[1]+styleBorderLine(parts[2],bottomColor,bottomDim,imports);
  }else if(showBottom)
    bottomBorder=styleBorderLine(bottomLine,bottomColor,bottomDim,imports);
  const auto offsetY=showTop?1:0;
  if(topBorder&&!topBorder->empty())output.write(x,y,*topBorder);
  if(showLeft)output.write(x,y+offsetY,leftBorder);
  if(showRight)output.write(x+width-1,y+offsetY,rightBorder);
  if(bottomBorder&&!bottomBorder->empty())
    output.write(x,y+height-1,*bottomBorder);
}
} // namespace batbox::terminal_render

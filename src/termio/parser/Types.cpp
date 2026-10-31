#include "termio/parser/Types.hpp"
#include <cmath>
#include <type_traits>

namespace batbox::termio::parser {
namespace {
Json number(double value){return std::isfinite(value)?Json(value):Json();}
}
TextStyle defaultStyle(){return {};}
Json toJson(const Color& color){
  Json result={{"type",color.type}};
  if(color.type=="named")result["name"]=color.name;
  else if(color.type=="indexed")result["index"]=number(color.index);
  else if(color.type=="rgb"){
    result["r"]=number(color.r);result["g"]=number(color.g);result["b"]=number(color.b);
  }
  return result;
}
Json toJson(const TextStyle& style){
  return Json{{"bold",style.bold},{"dim",style.dim},{"italic",style.italic},
    {"underline",style.underline},{"blink",style.blink},{"inverse",style.inverse},
    {"hidden",style.hidden},{"strikethrough",style.strikethrough},
    {"overline",style.overline},{"fg",toJson(style.fg)},{"bg",toJson(style.bg)},
    {"underlineColor",toJson(style.underlineColor)}};
}
Json toJson(const Action& action){
  return std::visit([](const auto& value)->Json{
    using T=std::decay_t<decltype(value)>;
    if constexpr(std::is_same_v<T,TextAction>){
      Json graphemes=Json::array();
      for(const auto& g:value.graphemes)graphemes.push_back({{"value",g.value},{"width",g.width}});
      return Json{{"type","text"},{"graphemes",graphemes},{"style",toJson(value.style)}};
    }else if constexpr(std::is_same_v<T,CursorAction>){
      Json nested={{"type",value.type}};
      if(value.type=="move"){
        nested["direction"]=value.direction;nested["count"]=number(value.count);
      }else if(value.type=="position"){
        nested["row"]=number(value.row);nested["col"]=number(value.col);
      }else if(value.type=="column")nested["col"]=number(value.col);
      else if(value.type=="row")nested["row"]=number(value.row);
      else if(value.type=="style"){
        nested["style"]=value.style;nested["blinking"]=value.blinking;
      }else if(value.type=="nextLine"||value.type=="prevLine")nested["count"]=number(value.count);
      return Json{{"type","cursor"},{"action",nested}};
    }else if constexpr(std::is_same_v<T,EraseAction>){
      Json nested={{"type",value.type}};
      if(value.type=="chars")nested["count"]=number(value.count);
      else nested["region"]=value.region;
      return Json{{"type","erase"},{"action",nested}};
    }else if constexpr(std::is_same_v<T,ScrollAction>){
      Json nested={{"type",value.type}};
      if(value.type=="setRegion"){
        nested["top"]=number(value.top);nested["bottom"]=number(value.bottom);
      }else nested["count"]=number(value.count);
      return Json{{"type","scroll"},{"action",nested}};
    }else if constexpr(std::is_same_v<T,ModeAction>){
      Json nested={{"type",value.type}};
      if(value.type=="mouseTracking")nested["mode"]=value.mode;
      else nested["enabled"]=value.enabled;
      return Json{{"type","mode"},{"action",nested}};
    }else if constexpr(std::is_same_v<T,LinkAction>){
      Json nested={{"type",value.type}};
      if(value.type=="start"){
        nested["url"]=value.url;
        if(value.params)nested["params"]=*value.params;
      }
      return Json{{"type","link"},{"action",nested}};
    }else if constexpr(std::is_same_v<T,TitleAction>){
      Json nested={{"type",value.type}};
      if(value.type=="iconName")nested["name"]=value.value;
      else nested["title"]=value.value;
      return Json{{"type","title"},{"action",nested}};
    }else if constexpr(std::is_same_v<T,TabStatusAction>){
      Json nested=Json::object();
      if(value.indicator)nested["indicator"]=*value.indicator?toJson(**value.indicator):Json();
      if(value.status)nested["status"]=*value.status?Json(**value.status):Json();
      if(value.statusColor)nested["statusColor"]=*value.statusColor?toJson(**value.statusColor):Json();
      return Json{{"type","tabStatus"},{"action",nested}};
    }else if constexpr(std::is_same_v<T,SgrAction>)return Json{{"type","sgr"},{"params",value.params}};
    else if constexpr(std::is_same_v<T,BellAction>)return Json{{"type","bell"}};
    else if constexpr(std::is_same_v<T,ResetAction>)return Json{{"type","reset"}};
    else return Json{{"type","unknown"},{"sequence",value.sequence}};
  },action);
}
Json toJson(const std::vector<Action>& actions){
  Json result=Json::array();for(const auto& action:actions)result.push_back(toJson(action));
  return result;
}
}

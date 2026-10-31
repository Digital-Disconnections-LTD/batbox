#include "settings/schema/imports/MarketplaceInternal.hpp"
#include "settings/schema/Internal.hpp"
#include <stdexcept>

namespace batbox::settings::schema::imports {
Json marketplaceDescriptor();
namespace detail {
namespace {
using schema::detail::ParseResult;
Json childPath(const Json& path,const Json& part){
  Json result=path;result.push_back(part);return result;
}
void extend(Json& target,const Json& issues){
  for(const auto& issue:issues)target.push_back(issue);
}
ParseResult typeError(const std::string& type,const Json& input,
  bool present,const Json& path){
  Json issues=Json::array({Json{{"expected",type},{"code","invalid_type"},
    {"path",path},{"message","Invalid input: expected "+type+
      ", received "+(present?schema::detail::typeName(input):"undefined")}}});
  return {false,true,nullptr,issues};
}
}
ParseResult parseMarketplaceNode(const Json& node,const Json& input,
  bool present,const Json& path,const MainImports& imports){
  const auto type=node.at("type").get<std::string>();
  if(type=="optional"){
    if(!present)return {true,false,nullptr,Json::array()};
    return parseMarketplaceNode(node.at("inner"),input,true,path,imports);
  }
  if(type=="object"||type=="array"||type=="record"){
    if(!present||(type=="array"&&!input.is_array())||
        (type!="array"&&!input.is_object()))
      return typeError(type,input,present,path);
    Json output=type=="array"?Json::array():Json::object();
    Json issues=Json::array();
    if(type=="array"){
      for(std::size_t i=0;i<input.size();++i){
        auto item=parseMarketplaceNode(node.at("element"),input[i],true,
          childPath(path,i),imports);
        extend(issues,item.issues);
        if(item.valid&&item.present)output.push_back(item.value);
      }
    }else if(type=="record"){
      for(auto it=input.begin();it!=input.end();++it){
        auto item=parseMarketplaceNode(node.at("value"),it.value(),true,
          childPath(path,it.key()),imports);
        extend(issues,item.issues);
        if(item.valid&&item.present)output[it.key()]=item.value;
      }
    }else{
      for(const auto& field:node.at("fields")){
        const auto key=field[0].get<std::string>();
        const bool has=input.contains(key);
        auto item=parseMarketplaceNode(field[1],has?input[key]:Json(nullptr),
          has,childPath(path,key),imports);
        extend(issues,item.issues);
        if(item.valid&&item.present)output[key]=item.value;
      }
      if(node.value("passthrough",false))
        for(auto it=input.begin();it!=input.end();++it){
          bool known=false;
          for(const auto& field:node.at("fields"))
            if(field[0]==it.key()){known=true;break;}
          if(!known)output[it.key()]=it.value();
        }
    }
    if(node.contains("checks"))applyMarketplaceChecks(node,input,path,
      output,issues,imports.canonicalUrl);
    return {issues.empty(),true,issues.empty()?output:Json(nullptr),issues};
  }
  if(type=="union"){
    if(node.contains("discriminator")){
      if(!present||!input.is_object())
        return typeError("object",input,present,path);
      const auto key=node.at("discriminator").get<std::string>();
      for(const auto& option:node.at("options"))
        for(const auto& field:option.at("fields")){
          if(field[0]!=key)continue;
          for(const auto& value:field[1].at("values"))
            if(input.contains(key)&&input[key]==value)
              return parseMarketplaceNode(option,input,true,path,imports);
        }
      Json issues=Json::array({Json{{"code","invalid_union"},
        {"errors",Json::array()},{"note","No matching discriminator"},
        {"path",childPath(path,key)},{"message","Invalid input"}}});
      return {false,true,nullptr,issues};
    }
    Json errors=Json::array();
    for(const auto& option:node.at("options")){
      auto item=parseMarketplaceNode(option,input,present,Json::array(),imports);
      if(item.valid)return {true,item.present,item.value,Json::array()};
      errors.push_back(item.issues);
    }
    Json issues=Json::array({Json{{"code","invalid_union"},
      {"errors",errors},{"path",path},{"message","Invalid input"}}});
    return {false,true,nullptr,issues};
  }
  Json base=node;base.erase("checks");
  auto parsed=schema::detail::parseNode(base,input,present,path,{},imports);
  if(parsed.valid&&node.contains("checks")){
    applyMarketplaceChecks(node,input,path,parsed.value,parsed.issues,
      imports.canonicalUrl);
    parsed.valid=parsed.issues.empty();
    if(!parsed.valid)parsed.value=nullptr;
  }
  return parsed;
}
}
SchemaResult marketplaceSource(const Json& input,
  const UrlCanonicalizer& canonicalUrl){
  MainImports imports;imports.canonicalUrl=canonicalUrl;
  auto parsed=detail::parseMarketplaceNode(marketplaceDescriptor(),input,true,
    Json::array(),imports);
  return {parsed.valid,parsed.valid?parsed.value:Json(nullptr),parsed.issues};
}
}

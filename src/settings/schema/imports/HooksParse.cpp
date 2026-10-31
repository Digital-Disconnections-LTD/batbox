#include "settings/schema/imports/Hooks.hpp"
#include "settings/schema/Runtime.hpp"
#include "settings/schema/Internal.hpp"
#include <stdexcept>

namespace batbox::settings::schema::imports {
Json hookDescriptor();
namespace {
using detail::ParseResult;
Json childPath(const Json& path,const Json& part){
  Json result=path;result.push_back(part);return result;
}
void extend(Json& target,const Json& source){
  for(const auto& issue:source)target.push_back(issue);
}
Json keyIssue(const Json& keyNode,const std::string& key,const Json& path){
  const Json& values=keyNode.at("options")[0].at("values");
  std::string names;
  for(const auto& value:values){
    if(!names.empty())names+='|';
    names+='"'+value.get<std::string>()+'"';
  }
  Json choices=Json::array({Json{{"code","invalid_value"},
    {"values",values},{"path",Json::array()},
    {"message","Invalid option: expected one of "+names}}});
  Json never=Json::array({Json{{"expected","never"},
    {"code","invalid_type"},{"path",Json::array()},
    {"message","Invalid input: expected never, received string"}}});
  Json invalidUnion=Json{{"code","invalid_union"},
    {"errors",Json::array({choices,never})},{"path",Json::array()},
    {"message","Invalid input"}};
  return {{"origin","record"},{"code","invalid_key"},
    {"issues",Json::array({invalidUnion})},{"path",childPath(path,key)},
    {"message","Invalid key in record"}};
}
ParseResult parseHook(const Json& node,const Json& input,bool present,
  const Json& path,const MainImports& imports){
  const auto type=node.at("type").get<std::string>();
  if(type=="optional"){
    if(!present)return {true,false,nullptr,Json::array()};
    return parseHook(node.at("inner"),input,true,path,imports);
  }
  if(type=="object"||type=="array"||type=="record"){
    if(!present||(type=="array"&&!input.is_array())||
        (type!="array"&&!input.is_object()))
      return detail::parseNode(node,input,present,path,{},imports);
    Json output=type=="array"?Json::array():Json::object();
    Json issues=Json::array();
    if(type=="array"){
      for(std::size_t i=0;i<input.size();++i){
        auto item=parseHook(node.at("element"),input[i],true,
          childPath(path,i),imports);
        extend(issues,item.issues);
        if(item.valid&&item.present)output.push_back(item.value);
      }
    }else if(type=="record"){
      for(auto it=input.begin();it!=input.end();++it){
        if(node.contains("key")){
          bool allowed=false;
          for(const auto& value:node["key"]["options"][0]["values"])
            if(value==it.key()){allowed=true;break;}
          if(!allowed){issues.push_back(keyIssue(node["key"],it.key(),path));
            continue;}
        }
        auto item=parseHook(node.at("value"),it.value(),true,
          childPath(path,it.key()),imports);
        extend(issues,item.issues);
        if(item.valid&&item.present)output[it.key()]=item.value;
      }
    }else{
      for(const auto& field:node.at("fields")){
        const auto key=field[0].get<std::string>();
        const bool has=input.contains(key);
        auto item=parseHook(field[1],has?input[key]:Json(nullptr),has,
          childPath(path,key),imports);
        extend(issues,item.issues);
        if(item.valid&&item.present)output[key]=item.value;
      }
    }
    return {issues.empty(),true,issues.empty()?output:Json(nullptr),issues};
  }
  if(type=="union"&&node.contains("discriminator")){
    if(!present||!input.is_object()){
      Json issues=Json::array({Json{{"code","invalid_type"},
        {"expected","object"},{"path",path},
        {"message","Invalid input: expected object, received "+
          (present?detail::typeName(input):"undefined")}}});
      return {false,true,nullptr,issues};
    }
    const auto discriminator=node.at("discriminator").get<std::string>();
    for(const auto& option:node.at("options")){
      for(const auto& field:option.at("fields")){
        if(field[0]!=discriminator)continue;
        for(const auto& value:field[1].at("values"))
          if(input.contains(discriminator)&&input[discriminator]==value)
            return parseHook(option,input,true,path,imports);
      }
    }
    Json issues=Json::array({Json{{"code","invalid_union"},
      {"errors",Json::array()},{"note","No matching discriminator"},
      {"path",childPath(path,discriminator)},{"message","Invalid input"}}});
    return {false,true,nullptr,issues};
  }
  return detail::parseNode(node,input,present,path,{},imports);
}
}
SchemaResult hooks(const Json& input,
  const UrlCanonicalizer& canonicalUrl){
  MainImports imports;imports.canonicalUrl=canonicalUrl;
  auto parsed=parseHook(hookDescriptor(),input,true,Json::array(),imports);
  return {parsed.valid,parsed.valid?parsed.value:Json(nullptr),parsed.issues};
}
}

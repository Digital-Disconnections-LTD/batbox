#include "settings/schema/Runtime.hpp"
#include "settings/schema/Internal.hpp"
#include <stdexcept>

namespace batbox::settings::schema::detail {
namespace {
Json childPath(const Json& path,const Json& part){Json result=path;result.push_back(part);return result;}
void extend(Json& target,const Json& issues){
  for(const auto& issue:issues)target.push_back(issue);
}
ParseResult fromSchema(const SchemaResult& result,const Json& path){
  ParseResult parsed{result.success,true,result.data,Json::array()};
  appendIssues(parsed.issues,result.issues,path);return parsed;
}
}
void appendIssues(Json& target,const Json& source,const Json& prefix){
  for(const auto& issue:source){
    Json copy=issue,path=prefix;
    if(issue.contains("path"))for(const auto& part:issue["path"])
      path.push_back(part);
    copy["path"]=path;target.push_back(std::move(copy));
  }
}
ParseResult parseNode(const Json& node,const Json& input,bool present,
  const Json& path,const MainConfig& config,const MainImports& imports){
  const auto type=node.at("type").get<std::string>();
  if(type=="optional"){
    if(!present)return {true,false,nullptr,Json::array()};
    return parseNode(node.at("inner"),input,true,path,config,imports);
  }
  if(type=="catch"){
    auto result=parseNode(node.at("inner"),input,present,path,config,imports);
    if(result.valid)return result;
    return {true,false,nullptr,Json::array()};
  }
  if(type=="string"||type=="number"||type=="boolean"||
      type=="enum"||type=="literal")
    return parseScalar(node,input,present,path,config,imports);
  if(type=="native"){
    const auto name=node.at("name").get<std::string>();
    if(!present)throw std::runtime_error("Missing native settings field");
    if(name=="environmentVariables")
      return fromSchema(environmentVariables(input),path);
    if(name=="permissions")
      return fromSchema(permissions(input,imports.permissionRules),path);
    throw std::runtime_error("Unbound native settings schema: "+name);
  }
  if(type=="import"){
    const auto name=node.at("name").get<std::string>();
    const std::function<SchemaResult(const Json&)>* callback=nullptr;
    if(name=="hooks")callback=&imports.hooks;
    if(!callback||!*callback)
      throw std::runtime_error("Missing settings schema import: "+name);
    return fromSchema((*callback)(input),path);
  }
  if(type=="object"||type=="record"||type=="array"){
    const bool matches=present&&((type=="array"&&input.is_array())||
      (type!="array"&&input.is_object()));
    if(!matches){
      Json issues=Json::array();
      issues.push_back({{"expected",type},{"code","invalid_type"},
        {"path",path},{"message","Invalid input: expected "+type+
        ", received "+(present?typeName(input):"undefined")}});
      return {false,true,nullptr,issues};
    }
    Json output=type=="array"?Json::array():Json::object();
    Json issues=Json::array();
    if(type=="array"){
      for(std::size_t i=0;i<input.size();++i){
        auto item=parseNode(node.at("element"),input[i],true,
          childPath(path,i),config,imports);
        extend(issues,item.issues);if(item.valid&&item.present)
          output.push_back(item.value);
      }
    }else if(type=="record"){
      for(auto it=input.begin();it!=input.end();++it){
        auto item=parseNode(node.at("value"),it.value(),true,
          childPath(path,it.key()),config,imports);
        extend(issues,item.issues);if(item.valid&&item.present)
          output[it.key()]=item.value;
      }
    }else{
      for(const auto& field:node.at("fields")){
        const auto key=field[0].get<std::string>();
        const bool has=input.contains(key);
        auto item=parseNode(field[1],has?input[key]:Json(nullptr),has,
          childPath(path,key),config,imports);
        extend(issues,item.issues);
        if(item.valid&&item.present)output[key]=item.value;
      }
      if(node.value("passthrough",false))
        for(auto it=input.begin();it!=input.end();++it){
          bool declared=false;
          for(const auto& field:node.at("fields"))
            if(field[0]==it.key()){declared=true;break;}
          if(!declared)output[it.key()]=it.value();
        }
    }
    return {issues.empty(),true,issues.empty()?output:Json(nullptr),issues};
  }
  if(type=="union"){
    Json branchErrors=Json::array();
    for(const auto& branch:node.at("options")){
      auto parsed=parseNode(branch,input,present,Json::array(),config,imports);
      if(parsed.valid)return {true,parsed.present,parsed.value,Json::array()};
      branchErrors.push_back(parsed.issues);
    }
    Json issues=Json::array({Json{{"code","invalid_union"},
      {"errors",branchErrors},{"path",path},{"message","Invalid input"}}});
    return {false,true,nullptr,issues};
  }
  throw std::runtime_error("Unbound settings schema type: "+type);
}
}

#include "gadgets/synthetic_output/IndigenousAjvDiagnostics.hpp"
#include <algorithm>
#include <cctype>
#include <regex>

namespace batbox::gadgets::synthetic_output {
namespace {
std::string decodeSegment(std::string text){
  std::string result;
  for(std::size_t i=0;i<text.size();++i){
    if(text[i]=='~'&&i+1<text.size()){
      if(text[i+1]=='0'){result+='~';++i;continue;}
      if(text[i+1]=='1'){result+='/';++i;continue;}
    }
    result+=text[i];
  }
  return result;
}
const Json* resolve(const Json& root,const Json* node){
  for(int depth=0;node&&node->is_object()&&depth<16;++depth){
    const auto& ref=value::property(*node,"$ref");
    if(!ref.is_string())break;
    const auto text=ref.get<std::string>();
    if(text.empty()||text[0]!='#')break;
    try{node=&root.at(Json::json_pointer(text.substr(1)));}
    catch(...){break;}
  }
  return node;
}
const Json* schemaAtInstancePath(const Json& root,const std::string& path){
  const Json* node=&root;
  std::size_t start=path.empty()?path.size():1;
  while(start<path.size()){
    const auto end=path.find('/',start);
    const auto segment=decodeSegment(path.substr(start,end-start));
    node=resolve(root,node);
    if(!node||!node->is_object())return nullptr;
    if(const auto& properties=value::property(*node,"properties");
      properties.is_object()&&properties.contains(segment))
      node=&properties.at(segment);
    else if(const auto& patterns=value::property(*node,"patternProperties");
      patterns.is_object()){
      const Json* selected=nullptr;
      for(const auto& [pattern,subschema]:patterns.items()){
        try{if(std::regex_search(segment,std::regex(pattern))){
          selected=&subschema;break;}}
        catch(const std::regex_error&){}
      }
      if(selected)node=selected;
      else if(const auto& additional=value::property(*node,
        "additionalProperties");additional.is_object())node=&additional;
      else return nullptr;
    }
    else if(const auto& items=value::property(*node,"items");
      items.is_object())node=&items;
    else if(items.is_array()&&!segment.empty()&&
      std::all_of(segment.begin(),segment.end(),[](unsigned char c){
        return std::isdigit(c)!=0;})){
      const auto index=static_cast<std::size_t>(std::stoull(segment));
      node=index<items.size()?&items.at(index):nullptr;
    }else if(const auto& additional=value::property(*node,
      "additionalProperties");additional.is_object())node=&additional;
    else return nullptr;
    if(end==std::string::npos)break;
    start=end+1;
  }
  return resolve(root,node);
}
const Json* instanceAtPath(const Json& value,const std::string& path){
  if(path.empty())return &value;
  try{return &value.at(Json::json_pointer(path));}
  catch(...){return nullptr;}
}
std::optional<std::pair<std::size_t,std::size_t>> duplicatePair(
  const Json& input){
  if(!input.is_array())return {};
  for(std::size_t i=input.size();i-->1;){
    const auto later=nlohmann::json::parse(input.at(i).dump());
    for(std::size_t j=i;j-->0;)
      if(later==nlohmann::json::parse(input.at(j).dump()))
        return std::pair{j,i};
  }
  return {};
}
std::string typeMessage(const Json* node){
  if(!node||!node->is_object())return {};
  const auto& type=value::property(*node,"type");
  if(type.is_string())return "must be "+type.get<std::string>();
  if(type.is_array()){
    std::string names;
    for(const auto& item:type){
      if(!item.is_string())return {};
      if(!names.empty())names+=",";
      names+=item.get<std::string>();
    }
    return names.empty()?std::string{}:"must be "+names;
  }
  return {};
}
}
std::string renderIndigenousIssueAsAjv(const Json& schema,const Json& value,
  const AjvValidationIssue& issue){
  const auto* node=schemaAtInstancePath(schema,issue.instancePath);
  const auto& message=issue.message;
  if(message=="unexpected instance type")
    if(auto converted=typeMessage(node);!converted.empty())return converted;
  if(message.rfind("required property '",0)==0){
    const auto end=message.find("'",19);
    if(end!=std::string::npos){
      const auto missing=message.substr(19,end-19);
      if(!issue.instancePath.empty()&&schema.is_object()&&
        schema.contains("dependencies")){
        const auto trigger=decodeSegment(issue.instancePath.substr(1));
        if(schema.at("dependencies").is_object()&&
          schema.at("dependencies").contains(trigger))
          return "must have property "+missing+" when property "+
            trigger+" is present";
      }
      return "must have required property '"+missing+"'";
    }
  }
  if(message.rfind("validation failed for additional property '",0)==0)
    return "must NOT have additional properties";
  if(message=="array has too few items"&&node&&node->is_object()&&
    node->contains("minItems"))
    return "must NOT have fewer than "+node->at("minItems").dump()+" items";
  if(message=="array has too many items"&&node&&node->is_object()&&
    node->contains("maxItems"))
    return "must NOT have more than "+node->at("maxItems").dump()+" items";
  if(message=="instance not found in required enum")
    return "must be equal to one of the allowed values";
  if(message=="instance not const")return "must be equal to constant";
  if(message=="items have to be unique for this array"){
    const auto* instance=instanceAtPath(value,issue.instancePath);
    if(instance)if(auto pair=duplicatePair(*instance))
      return "must NOT have duplicate items (items ## "+
        std::to_string(pair->first)+" and "+
        std::to_string(pair->second)+" are identical)";
  }
  if(message=="too many properties"&&node&&node->is_object()&&
    node->contains("maxProperties"))
    return "must NOT have more than "+node->at("maxProperties").dump()+
      " properties";
  if(message=="the subschema has succeeded, but it is required to not validate")
    return "must NOT be valid";
  if(message.rfind("instance is too long as per maxLength:",0)==0&&
    node&&node->is_object()&&node->contains("maxLength"))
    return "must NOT have more than "+node->at("maxLength").dump()+
      " characters";
  if(message.rfind("instance is too short as per minLength:",0)==0&&
    node&&node->is_object()&&node->contains("minLength"))
    return "must NOT have fewer than "+node->at("minLength").dump()+
      " characters";
  if(message.rfind("instance does not match regex pattern:",0)==0&&
    node&&node->is_object()&&node->contains("pattern"))
    return "must match pattern "+node->at("pattern").dump();
  if(message.rfind("instance is below minimum of ",0)==0&&
    node&&node->is_object()&&node->contains("minimum"))
    return "must be >= "+node->at("minimum").dump();
  if(message.rfind("instance is above maximum of ",0)==0&&
    node&&node->is_object()&&node->contains("maximum"))
    return "must be <= "+node->at("maximum").dump();
  if(message.rfind("instance is below or equals minimum of ",0)==0&&
    node&&node->is_object()&&node->contains("exclusiveMinimum"))
    return "must be > "+node->at("exclusiveMinimum").dump();
  if(message.rfind("instance is above or equals maximum of ",0)==0&&
    node&&node->is_object()&&node->contains("exclusiveMaximum"))
    return "must be < "+node->at("exclusiveMaximum").dump();
  if(message.rfind("instance is not a multiple of ",0)==0&&
    node&&node->is_object()&&node->contains("multipleOf"))
    return "must be multiple of "+node->at("multipleOf").dump();
  return message;
}
namespace {
std::optional<std::string> strictFormatAt(const Json& schema,
  const std::string& path){
  if(!schema.is_object())return {};
  if(const auto& format=value::property(schema,"format");format.is_string())
    return "unknown format \""+format.get<std::string>()+
      "\" ignored in schema at path \""+path+"\"";
  for(const char* key:{"$defs","definitions","properties",
    "patternProperties"}){
    const auto& map=value::property(schema,key);
    if(!map.is_object())continue;
    for(const auto& [name,subschema]:map.items())
      if(auto error=strictFormatAt(subschema,path+"/"+key+"/"+name))
        return error;
  }
  for(const char* key:{"additionalProperties","additionalItems",
    "propertyNames","contains","not","if","then","else","items"}){
    const auto& value=value::property(schema,key);
    if(value.is_array()){
      for(std::size_t i=0;i<value.size();++i)
        if(auto error=strictFormatAt(value.at(i),path+"/"+key+"/"+
          std::to_string(i)))return error;
    }else if(value.is_object())
      if(auto error=strictFormatAt(value,path+"/"+key))return error;
  }
  for(const char* key:{"allOf","anyOf","oneOf"}){
    const auto& array=value::property(schema,key);
    if(!array.is_array())continue;
    for(std::size_t i=0;i<array.size();++i)
      if(auto error=strictFormatAt(array.at(i),path+"/"+key+"/"+
        std::to_string(i)))return error;
  }
  return {};
}
}
std::optional<std::string> ajvStrictFormatError(const Json& schema){
  return strictFormatAt(schema,"#");
}
std::optional<std::string> ajvMetaErrorText(const Json& schema,
  const std::string& indigenousText){
  if(schema.is_object()&&schema.contains("type")&&
    schema.at("type").is_string()&&
    indigenousText.find("data/type")!=std::string::npos)
    return "data/type must be equal to one of the allowed values, "
      "data/type must be array, data/type must match a schema in anyOf";
  return {};
}
} // namespace batbox::gadgets::synthetic_output

#include "gadgets/synthetic_output/IndigenousJsonSchemaBackend.hpp"
#include "gadgets/synthetic_output/IndigenousAjvDiagnostics.hpp"
#include "gadgets/synthetic_output/IndigenousStrictTypes.hpp"
#include <nlohmann/json-schema.hpp>
#include <algorithm>
#include <set>

namespace batbox::gadgets::synthetic_output {
namespace {
using IndigenousJson=nlohmann::json;
IndigenousJson indigenous(const Json& value){return IndigenousJson::parse(value.dump());}
struct CollectErrors: nlohmann::json_schema::error_handler {
  std::vector<AjvValidationIssue> errors;
  void error(const IndigenousJson::json_pointer& pointer,const IndigenousJson&,
    const std::string& message)override{
    errors.push_back({pointer.to_string(),message});
  }
};
const std::set<std::string>& keywords(){
  static const std::set<std::string> names={
    "$schema","$id","$defs","$vocabulary","$comment","definitions",
    "$ref","id","type","nullable","const","enum","multipleOf",
    "maximum","exclusiveMaximum","minimum","exclusiveMinimum",
    "maxLength","minLength","pattern","maxProperties","minProperties",
    "required","maxItems","minItems","uniqueItems","not","anyOf",
    "oneOf","allOf","if","then","else","propertyNames",
    "additionalProperties","dependencies","properties","patternProperties",
    "additionalItems","items","contains","format","title","description",
    "default","deprecated","readOnly","writeOnly","examples",
    "contentMediaType","contentEncoding","contentSchema"};
  return names;
}
std::optional<std::string> unknownKeyword(const Json& schema){
  if(!schema.is_object())return {};
  for(auto it=schema.begin();it!=schema.end();++it){
    const auto& key=it.key();
    if(!keywords().contains(key))
      return "strict mode: unknown keyword: \""+key+"\"";
  }
  for(const char* key:{"$defs","definitions","properties",
    "patternProperties"}){
    if(!schema.contains(key)||!schema.at(key).is_object())continue;
    for(const auto& [name,subschema]:schema.at(key).items()){
      (void)name;
      if(auto unknown=unknownKeyword(subschema))return unknown;
    }
  }
  for(const char* key:{"additionalProperties","additionalItems",
    "propertyNames","contains","not","if","then","else","items"}){
    if(!schema.contains(key))continue;
    const auto& value=schema.at(key);
    if(value.is_array()){
      for(const auto& item:value)
        if(auto unknown=unknownKeyword(item))return unknown;
    }else if(auto unknown=unknownKeyword(value))return unknown;
  }
  if(schema.contains("dependencies")&&schema.at("dependencies").is_object())
    for(const auto& [name,value]:schema.at("dependencies").items()){
      (void)name;
      if(value.is_object()||value.is_boolean())
        if(auto unknown=unknownKeyword(value))return unknown;
    }
  for(const char* key:{"allOf","anyOf","oneOf"}){
    if(!schema.contains(key)||!schema.at(key).is_array())continue;
    for(const auto& item:schema.at(key))
      if(auto unknown=unknownKeyword(item))return unknown;
  }
  return {};
}
std::optional<std::string> normalizeNullable(Json& schema){
  if(!schema.is_object())return {};
  if(schema.contains("nullable")){
    if(!schema.contains("type"))
      return "\"nullable\" cannot be used without \"type\"";
    if(schema.at("nullable")==true){
      auto& type=schema["type"];
      if(type.is_string())type=Json::array({type,"null"});
      else if(type.is_array()&&std::find(type.begin(),type.end(),Json("null"))==
        type.end())type.push_back("null");
    }
  }
  for(const char* key:{"$defs","definitions","properties",
    "patternProperties"}){
    if(!schema.contains(key)||!schema.at(key).is_object())continue;
    for(auto& [name,subschema]:schema[key].items()){
      (void)name;
      if(auto error=normalizeNullable(subschema))return error;
    }
  }
  for(const char* key:{"additionalProperties","additionalItems",
    "propertyNames","contains","not","if","then","else","items"}){
    if(!schema.contains(key))continue;
    auto& value=schema[key];
    if(value.is_array()){
      for(auto& item:value)
        if(auto error=normalizeNullable(item))return error;
    }else if(auto error=normalizeNullable(value))return error;
  }
  if(schema.contains("dependencies")&&schema.at("dependencies").is_object())
    for(auto& [name,value]:schema["dependencies"].items()){
      (void)name;
      if(value.is_object()||value.is_boolean())
        if(auto error=normalizeNullable(value))return error;
    }
  for(const char* key:{"allOf","anyOf","oneOf"}){
    if(!schema.contains(key)||!schema.at(key).is_array())continue;
    for(auto& item:schema[key])
      if(auto error=normalizeNullable(item))return error;
  }
  return {};
}
std::optional<std::string> metaError(const IndigenousJson& schema){
  nlohmann::json_schema::json_validator meta(
    nlohmann::json_schema::draft7_schema_builtin,nullptr,
    [](const std::string&,const std::string&){});
  CollectErrors collector;
  meta.validate(schema,collector);
  if(collector.errors.empty())return {};
  std::string text;
  for(const auto& issue:collector.errors){
    if(!text.empty())text+=", ";
    text+="data"+issue.instancePath+" "+issue.message;
  }
  return text;
}
void normalizeIssue(const Json& schema,const Json& value,
  AjvValidationIssue& issue){
  const bool dependency=schema.is_object()&&
    schema.contains("dependencies")&&
    schema.at("dependencies").is_object()&&
    issue.instancePath.size()>1&&
    schema.at("dependencies").contains(issue.instancePath.substr(1))&&
    issue.message.rfind("required property '",0)==0;
  issue.message=renderIndigenousIssueAsAjv(schema,value,issue);
  if(dependency)issue.instancePath.clear();
}
std::vector<AjvValidationIssue> subErrors(const Json& subschema,
  const Json& value){
  Json normalized=subschema;
  if(auto error=normalizeNullable(normalized))
    throw std::invalid_argument(*error);
  nlohmann::json_schema::json_validator validator(indigenous(normalized));
  CollectErrors collector;
  validator.validate(indigenous(value),collector);
  for(auto& issue:collector.errors)normalizeIssue(subschema,value,issue);
  return collector.errors;
}
std::optional<std::vector<AjvValidationIssue>> composedErrors(
  const Json& schema,const Json& value){
  if(!schema.is_object())return {};
  for(const char* key:{"anyOf","oneOf","allOf"}){
    if(!schema.contains(key)||!schema.at(key).is_array())continue;
    std::vector<AjvValidationIssue> collected;
    std::size_t passed=0;
    for(const auto& item:schema.at(key)){
      auto failures=subErrors(item,value);
      if(failures.empty())++passed;
      else if(std::string(key)!="oneOf"||passed==0)
        collected.insert(collected.end(),failures.begin(),failures.end());
    }
    if(std::string(key)=="allOf"){
      if(passed<schema.at(key).size())return collected;
    }else if(std::string(key)=="anyOf"){
      if(passed==0){collected.push_back({{},
        "must match a schema in anyOf"});return collected;}
    }else if(passed!=1){
      if(passed>1)collected.clear();
      collected.push_back({{},"must match exactly one schema in oneOf"});
      return collected;
    }
  }
  if(schema.contains("contains")&&value.is_array()){
    std::vector<AjvValidationIssue> failures;
    bool passed=false;
    for(std::size_t i=0;i<value.size();++i){
      auto child=subErrors(schema.at("contains"),value.at(i));
      if(child.empty()){passed=true;break;}
      for(auto& issue:child){
        issue.instancePath="/"+std::to_string(i)+issue.instancePath;
        failures.push_back(std::move(issue));
      }
    }
    if(!passed){
      failures.push_back({{},"must contain at least 1 valid item(s)"});
      return failures;
    }
  }
  if(schema.contains("if")){
    const bool condition=subErrors(schema.at("if"),value).empty();
    const char* branch=condition?"then":"else";
    if(schema.contains(branch)){
      auto failures=subErrors(schema.at(branch),value);
      if(!failures.empty()){
        failures.push_back({{},std::string("must match \"")+branch+
          "\" schema"});
        return failures;
      }
    }
  }
  return {};
}
}
AjvBackend makeIndigenousJsonSchemaBackend(
  std::function<void(const std::string&)> logWarning){
  if(!logWarning)throw std::invalid_argument(
    "Local schema-warning logger required");
  AjvBackend backend;
  backend.validateSchema=[](const Json& schema)->std::optional<std::string>{
    try{
      const auto parsed=indigenous(schema);
      if(auto issue=metaError(parsed)){
        if(auto translated=ajvMetaErrorText(schema,*issue))return translated;
        return issue;
      }
      if(auto issue=unknownKeyword(schema))return issue;
      if(auto issue=ajvStrictFormatError(schema))return issue;
      Json normalized=schema;
      if(auto issue=normalizeNullable(normalized))return issue;
      nlohmann::json_schema::json_validator validator(indigenous(normalized));
      return {};
    }catch(const std::exception& error){return error.what();}
  };
  backend.compile=[logWarning=std::move(logWarning)](
    const Json& schema)->AjvValidator{
    emitAjvStrictTypeWarnings(schema,logWarning);
    Json normalized=schema;
    if(auto issue=normalizeNullable(normalized))
      throw std::invalid_argument(*issue);
    auto validator=std::make_shared<nlohmann::json_schema::json_validator>(
      indigenous(normalized));
    return [validator=std::move(validator),schema](const Json& value){
      CollectErrors collector;
      validator->validate(indigenous(value),collector);
      if(collector.errors.empty())return AjvValidation{true,{}};
      if(auto composed=composedErrors(schema,value))
        return AjvValidation{false,std::move(*composed)};
      for(auto& issue:collector.errors)normalizeIssue(schema,value,issue);
      return AjvValidation{false,std::move(collector.errors)};
    };
  };
  return backend;
}
} // namespace batbox::gadgets::synthetic_output

#include "gadgets/synthetic_output/IndigenousStrictTypes.hpp"
#include <array>

namespace batbox::gadgets::synthetic_output {
namespace {
struct Rule {const char* name;const char* expected;};
constexpr std::array rules={
  Rule{"maximum","number"},Rule{"minimum","number"},
  Rule{"exclusiveMaximum","number"},Rule{"exclusiveMinimum","number"},
  Rule{"multipleOf","number"},Rule{"maxLength","string"},
  Rule{"minLength","string"},Rule{"pattern","string"},
  Rule{"maxProperties","object"},Rule{"minProperties","object"},
  Rule{"required","object"},Rule{"maxItems","array"},
  Rule{"minItems","array"},Rule{"uniqueItems","array"},
  Rule{"propertyNames","object"},Rule{"additionalProperties","object"},
  Rule{"dependencies","object"},Rule{"properties","object"},
  Rule{"patternProperties","object"},Rule{"additionalItems","array"},
  Rule{"items","array"},Rule{"contains","array"}};
bool hasType(const Json& schema,const std::vector<std::string>& inherited,
  const std::string& type){
  const auto& value=value::property(schema,"type");
  if(value==type)return true;
  if(type=="number"&&value=="integer")return true;
  if(value.is_array())for(const auto& item:value)
    if(item==type||(type=="number"&&item=="integer"))return true;
  if(value.is_null())for(const auto& item:inherited)
    if(item==type||(type=="number"&&item=="integer"))return true;
  return false;
}
void visit(const Json& schema,const std::string& path,
  const std::function<void(const std::string&)>& warn,
  const std::vector<std::string>& inherited){
  if(!schema.is_object())return;
  std::vector<std::string> effective=inherited;
  const auto& declared=value::property(schema,"type");
  if(declared.is_string())effective={declared.get<std::string>()};
  else if(declared.is_array()){
    effective.clear();
    for(const auto& item:declared)
      if(item.is_string())effective.push_back(item.get<std::string>());
  }
  for(const auto& rule:rules){
    if(!schema.contains(rule.name)||
      hasType(schema,inherited,rule.expected))continue;
    warn(std::string("strict mode: missing type \"")+rule.expected+
      "\" for keyword \""+rule.name+"\" at \""+path+
      "\" (strictTypes)");
  }
  for(const char* key:{"not","anyOf","oneOf","allOf","if","then",
    "else","propertyNames","additionalProperties","dependencies",
    "properties","patternProperties","additionalItems","items",
    "contains"}){
    const auto& child=value::property(schema,key);
    if(child.is_array()){
      for(std::size_t i=0;i<child.size();++i)
        visit(child.at(i),path+"/"+key+"/"+std::to_string(i),warn,
          (std::string(key)=="allOf"||std::string(key)=="anyOf"||
            std::string(key)=="oneOf"||std::string(key)=="not")?
              effective:std::vector<std::string>{});
    }else if(child.is_object()){
      if(std::string(key)=="properties"||
        std::string(key)=="patternProperties"||
        std::string(key)=="dependencies"){
        for(const auto& [name,nested]:child.items())
          visit(nested,path+"/"+key+"/"+name,warn,{});
      }else visit(child,path+"/"+key,warn,
        (std::string(key)=="if"||std::string(key)=="then"||
          std::string(key)=="else"||std::string(key)=="not")?
            effective:std::vector<std::string>{});
    }
  }
}
}
void emitAjvStrictTypeWarnings(const Json& schema,
  const std::function<void(const std::string&)>& warn){
  if(!warn)return;
  visit(schema,"#",warn,{});
}
} // namespace batbox::gadgets::synthetic_output

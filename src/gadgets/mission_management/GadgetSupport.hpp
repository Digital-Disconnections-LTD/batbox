#pragma once
#include "gadgets/mission_management/Gadgets.hpp"
#include "runtime/AsyncLaunch.hpp"
#include "runtime/JsonValue.hpp"
#include <future>
#include <set>
#include <stdexcept>

namespace batbox::mission_management::detail {
template<class T>std::future<T> ready(T value){
  std::promise<T> promise;auto result=promise.get_future();
  promise.set_value(std::move(value));return result;
}
inline Json resultBlock(const std::string& text,const std::string& useId){
  return {{"tool_use_id",useId},{"type","tool_result"},{"content",text}};
}
inline std::string string(const Json& item,const char* field){
  const auto& value=value::property(item,field);
  return value.is_string()?value.get<std::string>():std::string{};
}
inline std::string listId(const Services& services){
  if(!services.missionListId)throw std::logic_error("Task list ID provider required");
  return services.missionListId();
}
inline void checkFields(const Json& input,const std::string& name,
    std::initializer_list<const char*> keys){
  if(!input.is_object())throw std::invalid_argument(name+" input must be an object");
  std::set<std::string> allowed(keys.begin(),keys.end());
  for(auto it=input.begin();it!=input.end();++it)
    if(!allowed.contains(it.key()))throw std::invalid_argument(
      "Unrecognized "+name+" input field: "+it.key());
}
inline void stringField(const Json& input,const char* key,bool required=false){
  if(!input.contains(key)){
    if(required)throw std::invalid_argument(std::string(key)+" must be a string");
    return;
  }
  if(!input.at(key).is_string())throw std::invalid_argument(
    std::string(key)+" must be a string");
}
inline void stringArray(const Json& input,const char* key){
  if(!input.contains(key))return;
  if(!input.at(key).is_array())throw std::invalid_argument(
    std::string(key)+" must be an array of strings");
  for(const auto& item:input.at(key))if(!item.is_string())
    throw std::invalid_argument(std::string(key)+" must be an array of strings");
}
inline void metadata(const Json& input){
  if(input.contains("metadata")&&!input.at("metadata").is_object())
    throw std::invalid_argument("metadata must be an object");
}
inline std::string join(const std::vector<std::string>& parts,
    const std::string& delimiter){
  std::string result;
  for(const auto& part:parts){if(!result.empty())result+=delimiter;result+=part;}
  return result;
}
inline std::vector<std::string> strings(const Json& value){
  std::vector<std::string> result;
  if(value.is_array())for(const auto& item:value)
    if(item.is_string())result.push_back(item.get<std::string>());
  return result;
}
inline gadgets::Gadget base(std::shared_ptr<Services> services,const char* name,
    const char* hint,const char* description,
    const std::function<std::string()>& poetry){
  gadgets::Gadget gadget;gadget.name=name;gadget.searchHint=hint;
  gadget.maxResultSizeChars=100000;gadget.shouldDefer=true;
  gadget.isEnabled=[services]{return services->enabled;};
  gadget.isConcurrencySafe=[](const Json&){return true;};
  gadget.userFacingName=[name](const Json&){return std::string(name);};
  gadget.description=[description](const Json&,const Json&){
    return ready(std::string(description));};
  gadget.poetry=[poetry](const gadgets::PoetryContext&){return ready(poetry());};
  gadget.renderGadgetUseMessage=[](const Json&,const Json&)->Json{return nullptr;};
  gadget.renderGadgetResultMessage=[](const Json& output,const Json&,const Json&)->Json{
    return output.is_string()?output:Json(output.dump(2));};
  return gadget;
}
} // namespace batbox::mission_management::detail

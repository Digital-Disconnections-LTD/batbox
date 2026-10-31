#include "react_sdk/Value.hpp"
#include "runtime/JsonValue.hpp"
#include <mutex>
#include <stdexcept>
#include <unordered_map>

namespace batbox::react_sdk {
namespace {
ValueRef make(Kind kind){auto value=std::make_shared<Value>();value->kind=kind;return value;}
}
ValueRef undefined(){static auto value=make(Kind::Undefined);return value;}
ValueRef nullValue(){static auto value=make(Kind::Null);return value;}
ValueRef boolean(bool input){auto value=make(Kind::Boolean);value->boolean=input;return value;}
ValueRef number(double input){auto value=make(Kind::Number);value->number=input;return value;}
ValueRef string(std::string input){auto value=make(Kind::String);value->text=std::move(input);return value;}
ValueRef bigInt(std::string decimal){auto value=make(Kind::BigInt);value->text=std::move(decimal);return value;}
ValueRef object(PropsRef input){auto value=make(Kind::Object);value->object=std::move(input);return value;}
ValueRef array(std::vector<ValueRef> input){auto value=make(Kind::Array);value->array=
  std::make_shared<std::vector<ValueRef>>(std::move(input));return value;}
ValueRef function(FunctionRef input){auto value=make(Kind::Function);value->function=std::move(input);return value;}
ValueRef hostObject(std::shared_ptr<void> input){
  auto value=make(Kind::HostObject);value->hostObject=std::move(input);return value;}
ValueRef hostFunction(std::shared_ptr<void> callback,std::string name){
  auto details=std::make_shared<Function>();
  details->name=std::move(name);
  details->hostCallback=std::move(callback);
  return function(std::move(details));
}
ValueRef portal(std::optional<std::string> key){
  auto value=make(Kind::Portal);value->key=std::move(key);return value;
}
ValueRef lazy(std::shared_ptr<void> payload){
  auto value=make(Kind::Lazy);value->hostObject=std::move(payload);return value;
}
ValueRef thenable(std::shared_ptr<void> payload){
  auto value=make(Kind::Thenable);value->hostObject=std::move(payload);return value;
}
ValueRef iterable(std::shared_ptr<void> payload){
  auto value=make(Kind::Iterable);value->hostObject=std::move(payload);return value;
}
ValueRef element(ElementRef input){auto value=make(Kind::Element);value->element=std::move(input);return value;}
ValueRef symbolFor(const std::string& key){
  static std::mutex mutex;
  static std::unordered_map<std::string,ValueRef> values;
  std::lock_guard lock(mutex);
  const auto found=values.find(key);
  if(found!=values.end())return found->second;
  auto value=make(Kind::Symbol);value->text=key;
  values.emplace(key,value);return value;
}
PropsRef props(){return std::make_shared<Props>();}
bool isUndefined(const ValueRef& value){return !value||value->kind==Kind::Undefined;}
bool truthy(const ValueRef& value){
  if(!value)return false;
  switch(value->kind){
    case Kind::Undefined:case Kind::Null:return false;
    case Kind::Boolean:return value->boolean;
    case Kind::Number:return value->number!=0&&!std::isnan(value->number);
    case Kind::String:return !value->text.empty();
    case Kind::BigInt:return value->text!="0"&&value->text!="-0";
    default:return true;
  }
}
bool sameValue(const ValueRef& a,const ValueRef& b){
  if(a==b)return true;
  if(!a||!b)return isUndefined(a)&&isUndefined(b);
  if(a->kind!=b->kind)return false;
  switch(a->kind){
    case Kind::Undefined:case Kind::Null:return true;
    case Kind::Boolean:return a->boolean==b->boolean;
    case Kind::Number:
      if(std::isnan(a->number)&&std::isnan(b->number))return true;
      if(a->number==0&&b->number==0)
        return std::signbit(a->number)==std::signbit(b->number);
      return a->number==b->number;
    case Kind::String:case Kind::BigInt:return a->text==b->text;
    case Kind::Symbol:return a==b;
    default:return false;
  }
}
std::string toString(const ValueRef& value){
  if(!value||value->kind==Kind::Undefined)return "undefined";
  switch(value->kind){
    case Kind::Null:return "null";
    case Kind::Boolean:return value->boolean?"true":"false";
    case Kind::Number:return value::numberToString(value->number);
    case Kind::String:case Kind::BigInt:return value->text;
    case Kind::Object:
      if(value->object&&value->object->toStringHook)
        return value->object->toStringHook();
      return "[object Object]";
    case Kind::Array:{
      std::string output;
      if(value->array)for(std::size_t i=0;i<value->array->size();++i){
        if(i)output+=',';
        const auto& item=(*value->array)[i];
        if(item&&item->kind!=Kind::Null&&!isUndefined(item))
          output+=toString(item);
      }
      return output;
    }
    case Kind::Function:
      if(value->function&&value->function->toStringHook)
        return value->function->toStringHook();
      throw std::logic_error("Function string coercion import required");
    case Kind::Symbol:
      throw std::runtime_error("Cannot convert a Symbol value to a string");
    default:return "[object Object]";
  }
}
std::shared_ptr<void> toOpaqueNode(const ValueRef& value){return value;}
ValueRef fromOpaqueNode(const std::shared_ptr<void>& value){
  return std::static_pointer_cast<Value>(value);
}
}

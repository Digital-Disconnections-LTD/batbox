#pragma once
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace batbox::react_sdk {
struct Value;
struct Props;
struct Function;
struct Element;
struct ContextData;
using ValueRef=std::shared_ptr<Value>;
using PropsRef=std::shared_ptr<Props>;
using FunctionRef=std::shared_ptr<Function>;
using ElementRef=std::shared_ptr<Element>;
using ArrayRef=std::shared_ptr<std::vector<ValueRef>>;

enum class Kind {Undefined,Null,Boolean,Number,String,Object,Array,
  Function,Element,Context,Consumer,Symbol,HostObject,
  BigInt,Portal,Lazy,Thenable,Iterable};
struct Value {
  Kind kind=Kind::Undefined;
  bool boolean=false;
  double number=0;
  std::string text;
  PropsRef object;
  ArrayRef array;
  FunctionRef function;
  ElementRef element;
  std::shared_ptr<ContextData> context;
  std::weak_ptr<Value> contextBackref;
  std::shared_ptr<void> hostObject;
  std::optional<std::string> key;
};
struct Property {
  std::string name;
  ValueRef value;
  bool enumerable=true;
};
struct Props {
  std::vector<Property> fields;
  PropsRef prototype;
  std::function<std::string()> toStringHook;
  bool hasOwn(const std::string& name)const;
  bool has(const std::string& name)const;
  ValueRef getOwn(const std::string& name)const;
  ValueRef get(const std::string& name)const;
  void set(std::string name,ValueRef value,bool enumerable=true);
  std::vector<std::string> ownEnumerableKeys()const;
  std::vector<std::string> enumerableKeys()const;
};
struct Function {
  std::string name;
  bool isReactComponent=false;
  PropsRef defaultProps;
  std::function<ValueRef(const std::vector<ValueRef>&)> invoke;
  std::function<std::string()> toStringHook;
  std::shared_ptr<void> hostCallback;
};
struct Element {
  ValueRef type;
  std::optional<std::string> key;
  PropsRef props;
  ValueRef ref;
  ValueRef owner;
};
struct ContextData {
  ValueRef defaultValue,currentValue,currentValue2,consumer;
  std::uint32_t threadCount=0;
  ValueRef currentRenderer,currentRenderer2;
};

ValueRef undefined();
ValueRef nullValue();
ValueRef boolean(bool value);
ValueRef number(double value);
ValueRef string(std::string value);
ValueRef bigInt(std::string decimal);
ValueRef object(PropsRef value);
ValueRef array(std::vector<ValueRef> values);
ValueRef function(FunctionRef value);
ValueRef hostObject(std::shared_ptr<void> value);
ValueRef hostFunction(std::shared_ptr<void> callback,
  std::string name={});
ValueRef portal(std::optional<std::string> key={});
ValueRef lazy(std::shared_ptr<void> payload);
ValueRef thenable(std::shared_ptr<void> payload);
ValueRef iterable(std::shared_ptr<void> payload);
ValueRef element(ElementRef value);
ValueRef symbolFor(const std::string& key);
PropsRef props();
bool isUndefined(const ValueRef& value);
bool truthy(const ValueRef& value);
bool sameValue(const ValueRef& a,const ValueRef& b);
std::string toString(const ValueRef& value);
std::shared_ptr<void> toOpaqueNode(const ValueRef& value);
ValueRef fromOpaqueNode(const std::shared_ptr<void>& value);
}

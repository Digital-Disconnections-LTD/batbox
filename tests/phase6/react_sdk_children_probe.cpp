#include "react_sdk/Children.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::react_sdk;
static Json summary(const ValueRef& value){
  if(isValidElement(value))return Json{{"kind","element"},
    {"key",value->element->key?Json(*value->element->key):Json(nullptr)},
    {"type",value->element->type->text}};
  if(!value||value->kind==Kind::Null)return Json{{"kind","null"},
    {"value","null"}};
  std::string kind="undefined";
  switch(value->kind){
    case Kind::Boolean:kind="boolean";break;
    case Kind::Number:kind="number";break;
    case Kind::String:kind="string";break;
    case Kind::BigInt:kind="bigint";break;
    default:kind="object";break;
  }
  return Json{{"kind",kind},{"value",toString(value)}};
}
static Json summaries(const std::vector<ValueRef>& values){
  Json result=Json::array();
  for(const auto& value:values)result.push_back(summary(value));
  return result;
}
int main(){
  Json rows=Json::array();
  auto pa=props();pa->set("key",string("a:b"));
  const auto a=element(jsx(string("one"),pa));
  const auto b=element(jsx(string("two"),props()));
  auto inner=array({b,string("text"),nullValue(),bigInt("42")});
  auto children=array({a,boolean(false),inner});
  rows.push_back({{"kind","toArray"},
    {"values",summaries(toArray(children))}});
  Json visited=Json::array();
  forEachChildren(children,[&](ValueRef child,std::size_t index){
    visited.push_back(Json::array({index,summary(child)}));
  });
  rows.push_back({{"kind","forEach"},{"values",visited},
    {"count",countChildren(children)}});
  const auto mapped=mapChildren(children,[&](ValueRef child,std::size_t index){
    if(index==0){auto p=props();p->set("key",string("new/key"));
      return element(jsx(string("mapped"),p));}
    if(index==2){auto p=props();p->set("key",string("slash/key"));
      return array({element(jsx(string("nested"),p)),string("extra")});}
    return child;
  });
  rows.push_back({{"kind","map"},{"values",summaries(*mapped->array)}});
  auto setValues=std::make_shared<std::vector<ValueRef>>(
    std::vector<ValueRef>{a,string("Q")});
  auto setValue=iterable(setValues);
  ChildrenImports imports;
  imports.getIterator=[&](const ValueRef& value){
    const auto items=std::static_pointer_cast<std::vector<ValueRef>>(
      value->hostObject);
    return std::function<std::optional<ValueRef>()>(
      [items,index=std::size_t{0}]()mutable->std::optional<ValueRef>{
        if(index>=items->size())return {};
        return (*items)[index++];
      });
  };
  rows.push_back({{"kind","iterable"},
    {"values",summaries(toArray(setValue,imports))}});
  auto resolved=element(jsx(string("resolved"),props()));
  auto promise=thenable(resolved);
  imports.resolveThenable=[](const ValueRef& value){
    return std::static_pointer_cast<Value>(value->hostObject);
  };
  rows.push_back({{"kind","thenable"},
    {"values",summaries(toArray(promise,imports))}});
  rows.push_back({{"kind","only"},{"same",onlyChild(a)==a}});
  for(const auto& invalid:{array({a}),object(props())}){
    try{(void)onlyChild(invalid);}
    catch(const std::exception& error){rows.push_back({{"kind","onlyError"},
      {"name","Error"},{"message",error.what()}});}
  }
  auto bad=props();bad->set("foo",number(1));bad->set("bar",number(2));
  try{(void)toArray(object(bad));}
  catch(const std::exception& error){rows.push_back({{"kind","objectError"},
    {"name","Error"},{"message",error.what()}});}
  std::cout<<rows.dump()<<'\n';
}

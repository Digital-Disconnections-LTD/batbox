#include "react_sdk/Context.hpp"
#include "react_sdk/Components.hpp"
#include "react_sdk/Element.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::react_sdk;
static Json keys(const PropsRef& value){
  return value->ownEnumerableKeys();
}
int main(){
  Json rows=Json::array();
  auto mark=object(props());
  auto plain=props();plain->set("title",string("plain"));plain->set("ref",mark);
  auto reused=jsx(string("ink-box"),plain);
  rows.push_back({{"kind","jsxReuse"},{"key",nullptr},
    {"sameProps",reused->props==plain},{"refSame",reused->ref==mark},
    {"keys",keys(reused->props)}});
  auto keyed=props();keyed->set("key",number(5));
  keyed->set("title",string("keyed"));keyed->set("ref",mark);
  auto copied=jsx(string("ink-box"),keyed,number(4));
  rows.push_back({{"kind","jsxKey"},{"key",copied->key.value()},
    {"sameProps",copied->props==keyed},{"refSame",copied->ref==mark},
    {"keys",keys(copied->props)}});
  auto undefKey=props();undefKey->set("key",undefined());
  undefKey->set("title",string("undef"));
  auto copiedUndefined=jsx(string("ink-box"),undefKey,number(9));
  rows.push_back({{"kind","jsxUndefinedKey"},
    {"key",copiedUndefined->key.value()},
    {"sameProps",copiedUndefined->props==undefKey},
    {"keys",keys(copiedUndefined->props)}});
  auto prototype=props();prototype->set("key",string("parent"));
  prototype->set("extra",string("inherited"));
  auto inherited=props();inherited->prototype=prototype;
  inherited->set("own",string("own"));
  auto inheritedElement=jsx(string("ink-box"),inherited);
  rows.push_back({{"kind","jsxInherited"},
    {"key",inheritedElement->key.value()},
    {"sameProps",inheritedElement->props==inherited},
    {"keys",keys(inheritedElement->props)},
    {"extra",toString(inheritedElement->props->get("extra"))}});
  auto component=std::make_shared<Function>();component->name="Component";
  component->defaultProps=props();
  component->defaultProps->set("defaulted",string("yes"));
  component->defaultProps->set("explicit",number(2));
  auto config=props();config->set("key",number(7));config->set("ref",mark);
  config->set("__self",string("x"));config->set("__source",string("y"));
  config->set("defaulted",undefined());config->set("explicit",number(4));
  auto created=createElement(function(component),config,{string("child")});
  rows.push_back({{"kind","createOne"},{"key",created->key.value()},
    {"sameProps",created->props==config},{"refSame",created->ref==mark},
    {"keys",keys(created->props)},
    {"child",toString(created->props->get("children"))},
    {"defaulted",toString(created->props->get("defaulted"))},
    {"explicit",created->props->get("explicit")->number}});
  auto many=createElement(string("ink-text"),{},
    {string("A"),string("B")});
  Json children=Json::array();
  for(const auto& item:*many->props->get("children")->array)
    children.push_back(toString(item));
  rows.push_back({{"kind","createMany"},{"key",nullptr},
    {"childArray",many->props->get("children")->kind==Kind::Array},
    {"children",children},{"valid",isValidElement(element(many))}});
  auto contextDefault=object(props());auto context=createContext(contextDefault);
  rows.push_back({{"kind","context"},
    {"providerSame",contextProvider(context)==context},
    {"consumerBack",contextConsumer(context)->contextBackref.lock()==context},
    {"defaultSame",context->context->currentValue==contextDefault},
    {"secondSame",context->context->currentValue2==contextDefault},
    {"threadCount",context->context->threadCount}});
  auto ref=createRef();
  rows.push_back({{"kind","ref"},{"current",nullptr},
    {"keys",keys(ref->object)}});
  rows.push_back({{"kind","validity"},
    {"element",isValidElement(element(reused))},
    {"object",isValidElement(object(props()))},
    {"nil",isValidElement(nullValue())}});
  auto componentValue=function(component);
  auto memoType=memo(componentValue);
  rows.push_back({{"kind","memo"},
    {"symbol",memoType->object->get("$$typeof")->text},
    {"sameType",memoType->object->get("type")==componentValue},
    {"compareNull",memoType->object->get("compare")->kind==Kind::Null},
    {"keys",keys(memoType->object)}});
  auto comparison=function(std::make_shared<Function>());
  auto customMemo=memo(componentValue,comparison);
  rows.push_back({{"kind","memoCompare"},
    {"sameCompare",customMemo->object->get("compare")==comparison}});
  auto forwarded=forwardRef(componentValue);
  rows.push_back({{"kind","forwardRef"},
    {"symbol",forwarded->object->get("$$typeof")->text},
    {"sameRender",forwarded->object->get("render")==componentValue},
    {"keys",keys(forwarded->object)},
    {"fragment",fragment()->text},{"suspense",suspense()->text}});
  auto cloneConfig=props();cloneConfig->set("key",string("clone"));
  cloneConfig->set("ref",undefined());
  cloneConfig->set("title",string("new"));
  cloneConfig->set("__self",string("ignored"));
  auto cloned=cloneElement(element(created),cloneConfig,{string("override")});
  rows.push_back({{"kind","clone"},{"key",cloned->key.value()},
    {"propsSame",cloned->props==created->props},
    {"refSame",cloned->ref==mark},
    {"keys",keys(cloned->props)},
    {"title",toString(cloned->props->get("title"))},
    {"child",toString(cloned->props->get("children"))}});
  auto inheritedClone=props();inheritedClone->prototype=props();
  inheritedClone->prototype->set("key",string("inherited"));
  inheritedClone->set("title",string("own"));
  auto clonedInherited=cloneElement(element(created),inheritedClone);
  rows.push_back({{"kind","cloneInherited"},
    {"key",clonedInherited->key.value()},
    {"title",toString(clonedInherited->props->get("title"))},
    {"child",toString(clonedInherited->props->get("children"))}});
  for(const auto& invalid:{nullValue(),undefined()}){
    try{(void)cloneElement(invalid);}
    catch(const std::exception& error){rows.push_back({{"kind","cloneError"},
      {"name","Error"},{"message",error.what()}});}
  }
  try{auto bad=props();bad->set("key",symbolFor("bad"));
    (void)jsx(string("ink-box"),bad);}
  catch(const std::exception& error){rows.push_back({{"kind","symbolKeyError"},
    {"name","TypeError"},{"message",error.what()}});}
  std::cout<<rows.dump()<<'\n';
}

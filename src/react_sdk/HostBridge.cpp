#include "react_sdk/HostBridge.hpp"
#include <stdexcept>

namespace batbox::react_sdk {
void HostBridge::prune(){
  for(auto it=projected_.begin();it!=projected_.end();)
    if(it->second.source.expired())it=projected_.erase(it);
    else ++it;
  for(auto it=styleRecords_.begin();it!=styleRecords_.end();)
    if(it->second.source.expired())it=styleRecords_.erase(it);
    else ++it;
  for(auto it=bundles_.begin();it!=bundles_.end();)
    if(it->second.source.expired())it=bundles_.erase(it);
    else ++it;
}
batbox::value::ValueRef HostBridge::projectJson(const ValueRef& value){
  if(!value)return std::make_shared<Json>(Json::value_t::discarded);
  if(const auto found=projected_.find(value.get());found!=projected_.end()){
    if(found->second.source.lock()==value)return found->second.projected;
    projected_.erase(found);
  }
  if(!projecting_.insert(value.get()).second)
    throw std::invalid_argument("Cyclic host JSON projection requires Fiber import");
  struct ProjectionScope {
    std::unordered_set<const Value*>& active;
    const Value* value;
    ~ProjectionScope(){active.erase(value);}
  } scope{projecting_,value.get()};
  Json result;
  switch(value->kind){
    case Kind::Undefined:result=Json(Json::value_t::discarded);break;
    case Kind::Null:result=nullptr;break;
    case Kind::Boolean:result=value->boolean;break;
    case Kind::Number:result=value->number;break;
    case Kind::String:result=value->text;break;
    case Kind::Object:
      result=Json::object();
      if(value->object)for(const auto& key:value->object->ownEnumerableKeys()){
        const auto projected=projectJson(value->object->getOwn(key));
        result[key]=*projected;
      }
      break;
    case Kind::Array:
      result=Json::array();
      if(value->array)for(const auto& item:*value->array)
        result.push_back(*projectJson(item));
      break;
    default:throw std::invalid_argument(
      "React host callback/element projection import required");
  }
  auto projected=std::make_shared<Json>(std::move(result));
  projected_.emplace(value.get(),Entry<Value,batbox::value::ValueRef>{value,projected});
  return projected;
}
batbox::value::RecordRef HostBridge::projectStyle(const ValueRef& value){
  if(!value||value->kind!=Kind::Object||!value->object)return {};
  if(const auto found=styleRecords_.find(value.get());
      found!=styleRecords_.end()){
    if(found->second.source.lock()==value)return found->second.projected;
    styleRecords_.erase(found);
  }
  auto record=std::make_shared<batbox::value::SharedRecord>();
  for(const auto& key:value->object->ownEnumerableKeys())
    record->setRef(key,projectJson(value->object->getOwn(key)));
  styleRecords_.emplace(value.get(),Entry<Value,batbox::value::RecordRef>{value,record});
  return record;
}
terminal_reconciler::PropsBundle HostBridge::project(
    const PropsRef& source){
  if(!source)throw std::invalid_argument("React props required");
  prune();
  if(const auto found=bundles_.find(source.get());found!=bundles_.end()){
    if(found->second.source.lock()==source)return found->second.projected;
    bundles_.erase(found);
  }
  terminal_reconciler::PropsBundle output;
  output.values=std::make_shared<batbox::value::SharedRecord>();
  for(const auto& key:source->ownEnumerableKeys()){
    const auto value=source->getOwn(key);
    terminal_reconciler::HostPropValue projected;
    if(value&&(value->kind==Kind::Function||
        value->kind==Kind::Element||value->kind==Kind::Context||
        value->kind==Kind::Consumer||value->kind==Kind::Symbol||
        value->kind==Kind::HostObject)){
      if(!imports_.projectSpecial)throw std::invalid_argument(
        "React host callback/element projection import required");
      projected=imports_.projectSpecial(key,value);
    }else projected.value=projectJson(value);
    output.values->setRef(key,projected.value);
    if(key=="style")output.style=projectStyle(value);
    if(projected.handler)output.handlers[key]=*projected.handler;
    if(projected.onClick)output.clickHandlers[key]=projected.onClick;
    if(projected.onHover)output.hoverHandlers[key]=projected.onHover;
  }
  bundles_.emplace(source.get(),
    Entry<Props,terminal_reconciler::PropsBundle>{source,output});
  return output;
}
}

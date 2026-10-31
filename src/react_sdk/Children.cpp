#include "react_sdk/Children.hpp"
#include <stdexcept>

namespace batbox::react_sdk {
namespace {
std::string escape(const std::string& key){
  std::string output="$";
  for(const char character:key){
    if(character=='=')output+="=0";
    else if(character==':')output+="=2";
    else output+=character;
  }
  return output;
}
std::string escapeUserProvidedKey(const std::string& text){
  std::string output;
  for(std::size_t index=0;index<text.size();){
    if(text[index]!='/'){output+=text[index++];continue;}
    do{output+=text[index++];}while(index<text.size()&&text[index]=='/');
    output+='/';
  }
  return output;
}
std::string radix36(std::size_t index){
  static constexpr char digits[]="0123456789abcdefghijklmnopqrstuvwxyz";
  std::string output;
  do{output.insert(output.begin(),digits[index%36]);index/=36;}while(index);
  return output;
}
std::optional<std::string> keyOf(const ValueRef& child){
  if(!child)return {};
  if(child->kind==Kind::Element&&child->element)return child->element->key;
  if(child->kind==Kind::Portal)return child->key;
  if(child->kind==Kind::Object&&child->object){
    const auto key=child->object->get("key");
    if(!isUndefined(key)&&key->kind!=Kind::Null)return toString(key);
  }
  return {};
}
std::string getElementKey(const ValueRef& child,std::size_t index){
  const auto key=keyOf(child);
  return key?escape(*key):radix36(index);
}
std::string invalidObjectError(const ValueRef& child){
  const auto description=toString(child);
  std::string found=description;
  if(description=="[object Object]"){
    found="object with keys {";
    if(child&&child->object){
      bool first=true;
      for(const auto& key:child->object->ownEnumerableKeys()){
        if(!first)found+=", ";
        first=false;found+=key;
      }
    }
    found+='}';
  }
  return "Objects are not valid as a React child (found: "+found+"). "
    "If you meant to render a collection of children, use an array instead.";
}
using Mapped=std::vector<ValueRef>;
std::size_t mapIntoArray(ValueRef children,Mapped& output,
    const std::string& escapedPrefix,const std::string& nameSoFar,
    const std::function<ValueRef(ValueRef)>& callback,
    const ChildrenImports& imports){
  auto type=children?children->kind:Kind::Undefined;
  if(type==Kind::Undefined||type==Kind::Boolean)children=nullValue();
  bool invoke=false;
  if(children&&children->kind==Kind::Null)invoke=true;
  else switch(type){
    case Kind::BigInt:case Kind::String:case Kind::Number:
    case Kind::Element:case Kind::Portal:invoke=true;break;
    case Kind::Lazy:
      if(!imports.resolveLazy)throw std::logic_error("React lazy import required");
      return mapIntoArray(imports.resolveLazy(children),output,
        escapedPrefix,nameSoFar,callback,imports);
    default:break;
  }
  if(invoke){
    const auto child=children;
    auto mappedChild=callback(child);
    const auto childKey=nameSoFar.empty()?"."+getElementKey(child,0):nameSoFar;
    if(mappedChild&&mappedChild->kind==Kind::Array){
      auto escapedChildKey=escapeUserProvidedKey(childKey)+"/";
      mapIntoArray(mappedChild,output,escapedChildKey,"",
        [](ValueRef item){return item;},imports);
    }else if(mappedChild&&mappedChild->kind!=Kind::Null&&
        mappedChild->kind!=Kind::Undefined){
      if(isValidElement(mappedChild)){
        const auto newKey=mappedChild->element->key;
        const auto oldKey=keyOf(child);
        mappedChild=element(cloneAndReplaceKey(mappedChild->element,
          escapedPrefix+(newKey&&oldKey!=newKey?
            escapeUserProvidedKey(*newKey)+"/":"")+childKey));
      }
      output.push_back(mappedChild);
    }
    return 1;
  }
  const auto nextNamePrefix=nameSoFar.empty()?".":nameSoFar+":";
  std::size_t subtreeCount=0;
  if(children&&children->kind==Kind::Array){
    if(children->array)for(std::size_t index=0;
        index<children->array->size();++index){
      const auto child=(*children->array)[index];
      subtreeCount+=mapIntoArray(child,output,escapedPrefix,
        nextNamePrefix+getElementKey(child,index),callback,imports);
    }
  }else if(children&&children->kind==Kind::Iterable){
    if(!imports.getIterator)throw std::logic_error("React iterator import required");
    auto iterator=imports.getIterator(children);
    for(std::size_t index=0;;++index){
      auto child=iterator();if(!child)break;
      subtreeCount+=mapIntoArray(*child,output,escapedPrefix,
        nextNamePrefix+getElementKey(*child,index),callback,imports);
    }
  }else if(children&&children->kind==Kind::Thenable){
    if(!imports.resolveThenable)
      throw std::logic_error("React thenable import required");
    return mapIntoArray(imports.resolveThenable(children),output,
      escapedPrefix,nameSoFar,callback,imports);
  }else if(children&&(children->kind==Kind::Object||
      children->kind==Kind::HostObject||children->kind==Kind::Context||
      children->kind==Kind::Consumer))
    throw std::runtime_error(invalidObjectError(children));
  return subtreeCount;
}
}
ValueRef mapChildren(ValueRef children,MapChild map,
    const ChildrenImports& imports){
  if(!children||children->kind==Kind::Null||children->kind==Kind::Undefined)
    return children;
  if(!map)throw std::invalid_argument("React child map required");
  auto result=std::vector<ValueRef>{};
  std::size_t count=0;
  mapIntoArray(children,result,"","",[&](ValueRef child){
    return map(child,count++);
  },imports);
  return array(std::move(result));
}
std::size_t countChildren(ValueRef children,const ChildrenImports& imports){
  std::size_t count=0;
  mapChildren(children,[&](ValueRef,std::size_t){
    ++count;return undefined();
  },imports);
  return count;
}
void forEachChildren(ValueRef children,
    const std::function<void(ValueRef,std::size_t)>& visit,
    const ChildrenImports& imports){
  mapChildren(children,[&](ValueRef child,std::size_t index){
    visit(child,index);return undefined();
  },imports);
}
std::vector<ValueRef> toArray(ValueRef children,
    const ChildrenImports& imports){
  auto mapped=mapChildren(children,[](ValueRef child,std::size_t){
    return child;
  },imports);
  if(!mapped||mapped->kind!=Kind::Array||!mapped->array)return {};
  return *mapped->array;
}
ValueRef onlyChild(ValueRef children){
  if(!isValidElement(children))throw std::runtime_error(
    "React.Children.only expected to receive a single React element child.");
  return children;
}
} // namespace batbox::react_sdk

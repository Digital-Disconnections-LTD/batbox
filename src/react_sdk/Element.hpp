#pragma once
#include "react_sdk/Value.hpp"

namespace batbox::react_sdk {
ElementRef reactElement(ValueRef type,std::optional<std::string> key,
  PropsRef elementProps,ValueRef owner=nullValue());
ElementRef jsx(ValueRef type,PropsRef config,
  std::optional<ValueRef> maybeKey={});
inline ElementRef jsxs(ValueRef type,PropsRef config,
    std::optional<ValueRef> maybeKey={}){
  return jsx(std::move(type),std::move(config),std::move(maybeKey));
}
ElementRef createElement(ValueRef type,PropsRef config={},
  std::vector<ValueRef> children={});
ElementRef cloneAndReplaceKey(const ElementRef& oldElement,
  const std::string& newKey);
ElementRef cloneElement(const ValueRef& original,PropsRef config={},
  std::vector<ValueRef> children={});
bool isValidElement(const ValueRef& value);
}

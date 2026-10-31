#pragma once
#include "react_sdk/Element.hpp"
#include <functional>

namespace batbox::react_sdk {
struct ChildrenImports {
  // These resolve public SDK object protocols; Fiber may supply them.
  std::function<ValueRef(const ValueRef&)> resolveLazy;
  std::function<ValueRef(const ValueRef&)> resolveThenable;
  std::function<std::function<std::optional<ValueRef>()>(const ValueRef&)>
    getIterator;
};
using MapChild=std::function<ValueRef(ValueRef,std::size_t)>;
ValueRef mapChildren(ValueRef children,MapChild map,
  const ChildrenImports& imports={});
std::size_t countChildren(ValueRef children,
  const ChildrenImports& imports={});
void forEachChildren(ValueRef children,
  const std::function<void(ValueRef,std::size_t)>& visit,
  const ChildrenImports& imports={});
std::vector<ValueRef> toArray(ValueRef children,
  const ChildrenImports& imports={});
ValueRef onlyChild(ValueRef children);
} // namespace batbox::react_sdk

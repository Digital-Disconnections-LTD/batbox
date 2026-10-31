#pragma once
#include "react_sdk/Value.hpp"
#include "terminal_reconciler/HostConfig.hpp"
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace batbox::react_sdk {
struct HostProjectionImports {
  // Fiber/host owns event callback conversion and element child traversal.
  // The callback must retain its input ValueRef when returning a callable.
  std::function<terminal_reconciler::HostPropValue(
    const std::string& key,const ValueRef& value)> projectSpecial;
};
class HostBridge {
public:
  explicit HostBridge(HostProjectionImports imports):imports_(std::move(imports)){}
  terminal_reconciler::PropsBundle project(const PropsRef& source);
private:
  template<class Source,class Projected> struct Entry {
    std::weak_ptr<Source> source;
    Projected projected;
  };
  HostProjectionImports imports_;
  std::unordered_map<const Value*,Entry<Value,batbox::value::ValueRef>> projected_;
  std::unordered_set<const Value*> projecting_;
  std::unordered_map<const Value*,Entry<Value,batbox::value::RecordRef>> styleRecords_;
  std::unordered_map<const Props*,Entry<Props,terminal_reconciler::PropsBundle>> bundles_;
  void prune();
  batbox::value::ValueRef projectJson(const ValueRef& value);
  batbox::value::RecordRef projectStyle(const ValueRef& value);
};
}

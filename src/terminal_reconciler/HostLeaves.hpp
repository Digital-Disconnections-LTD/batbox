#pragma once
#include "runtime/SharedRecord.hpp"
#include "terminal_dom/Dom.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace batbox::terminal_reconciler {
using Props=batbox::value::SharedRecord;
using PropsRef=batbox::value::RecordRef;
PropsRef diff(const PropsRef& before,const PropsRef& after);
void cleanupYogaNode(terminal_dom::DOMNode& node);
struct OwnerFiber;
using OwnerFiberRef=std::shared_ptr<OwnerFiber>;
struct OwnerFiber {
  enum class Type {String,Function,Object,Missing};
  Type type=Type::Missing;
  std::optional<std::string> displayName,name;
  OwnerFiberRef debugOwner,parent;
};
std::vector<std::string> getOwnerChain(const OwnerFiberRef& fiber);
class DebugRepaints {
public:
  explicit DebugRepaints(std::function<std::optional<std::string>()> env):
    env_(std::move(env)){}
  bool enabled();
private:
  std::function<std::optional<std::string>()> env_;
  std::optional<bool> stashed_;
};
class ProfileCounters {
public:
  explicit ProfileCounters(std::function<double()> now):now_(std::move(now)){}
  void recordYogaMs(double value);
  double getLastYogaMs()const;
  void markCommitStart();
  double getLastCommitMs()const;
  void resetProfileCounters();
  void finishCommit();
private:
  std::function<double()> now_;
  double lastYogaMs_=0,lastCommitMs_=0,commitStart_=0;
};
} // namespace batbox::terminal_reconciler

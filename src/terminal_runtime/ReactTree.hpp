#pragma once
#include "react_sdk/Value.hpp"
#include "terminal_dom/Node.hpp"
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <string>

namespace batbox::terminal_runtime {
struct InputPort;
struct OutputPort;
using FiberContainerRef=std::shared_ptr<void>;
using ReactNoopRef=std::shared_ptr<std::function<void()>>;

// Fiber supplies these operations. The four error/indicator callbacks share
// one imported no-op identity.
struct FiberCreateArgs {
  terminal_dom::ElementRef rootNode;
  int rootTag=1;
  bool strictMode=false;
  std::nullptr_t hydrationCallbacks=nullptr;
  std::nullptr_t concurrentUpdatesByDefault=nullptr;
  std::string identifierPrefix="id";
  ReactNoopRef onUncaughtError,onCaughtError,onRecoverableError,
    onDefaultTransitionIndicator;
};
struct FiberUpdateArgs {
  react_sdk::ValueRef node;
  FiberContainerRef container;
  std::nullptr_t parentComponent=nullptr;
  ReactNoopRef callback;
};
struct ReactTreeServices {
  react_sdk::ValueRef appComponent,terminalWriteProvider;
  std::shared_ptr<InputPort> stdin;
  std::shared_ptr<OutputPort> stdout,stderr;
  bool exitOnCtrlC=true;
  int concurrentRootTag=1;
  std::function<FiberContainerRef(const FiberCreateArgs&)> createContainer;
  std::function<void(const FiberUpdateArgs&)> updateContainerSync;
  std::function<void()> flushSyncWork;
  std::function<void()> flushSyncFromReconciler;
};
// A C++ Error object passed through a React host callback.
struct ReactError {std::exception_ptr error;};
} // namespace batbox::terminal_runtime

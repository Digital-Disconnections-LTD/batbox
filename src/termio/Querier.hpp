#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <memory>
namespace batbox::termio {
using ResponseRef=std::shared_ptr<Json>;
struct TerminalQuery {
  std::string request;
  std::function<bool(const ResponseRef&)> match;
};
namespace query {
TerminalQuery decrqm(double mode);
TerminalQuery da1();
TerminalQuery da2();
TerminalQuery kittyKeyboard();
TerminalQuery cursorPosition();
TerminalQuery oscColor(double code,const std::string& terminal);
TerminalQuery xtversion();
}
class TerminalQuerier {
public:
  TerminalQuerier(event_loop::Loop& loop,
    std::function<void(const std::string&)> stdoutWrite);
  event_loop::Deferred<ResponseRef> send(TerminalQuery query);
  event_loop::Deferred<event_loop::Unit> flush();
  void onResponse(ResponseRef response);
private:
  struct Pending {
    bool sentinel=false;
    std::function<bool(const ResponseRef&)> match;
    std::function<void(ResponseRef)> resolve;
    std::function<void()> resolveSentinel;
  };
  event_loop::Loop& loop_;
  std::function<void(const std::string&)> stdoutWrite_;
  std::vector<std::shared_ptr<Pending>> queue_;
};
}

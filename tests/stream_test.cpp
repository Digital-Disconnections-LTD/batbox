#include "utils/stream.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace batbox;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
  using namespace std::chrono_literals;
  try {
    Stream<std::string> stream;
    stream.iterate();
    bool twice = false;
    try { stream.iterate(); } catch (const std::runtime_error& e) { twice = std::string(e.what()) == "Stream can only be iterated once"; }
    require(twice, "single iterator contract");
    auto pending = stream.next();
    require(pending.wait_for(0s) == std::future_status::timeout, "empty stream waits");
    stream.enqueue("first");
    require(pending.get().value == "first", "enqueue resolves pending read");
    stream.enqueue("second");
    stream.error("failure");
    require(stream.next().get().value == "second", "queued values precede errors");
    bool rejected = false;
    try { stream.next().get(); } catch (const StreamError& e) { rejected = e.value == "failure"; }
    require(rejected, "error rejects next");
    stream.done();
    require(stream.next().get().done, "done precedes saved error");
    stream.enqueue("after done");
    require(stream.next().get().value == "after done", "queued values precede the completion signal");
    Stream<int> completed;
    auto ending = completed.next(); completed.done();
    require(ending.get().done, "done resolves pending read");
    Stream<int> falsy;
    auto rejectedFalsy = falsy.next(); falsy.error(false);
    rejected = false;
    try { rejectedFalsy.get(); } catch (const StreamError& e) { rejected = e.value == false; }
    require(rejected, "falsy error still rejects a waiting read");
    auto afterFalsy = falsy.next();
    require(afterFalsy.wait_for(0s) == std::future_status::timeout, "falsy saved error does not reject future reads");
    falsy.enqueue(7); require(afterFalsy.get().value == 7, "falsy error recovery");
    Stream<int> concurrent;
    auto lost = concurrent.next(); auto latest = concurrent.next();
    concurrent.enqueue(9);
    require(latest.get().value == 9 && lost.wait_for(0s) == std::future_status::timeout, "only the latest pending resolver receives a value");
    int returned = 0;
    Stream<int> cancelled([&] { ++returned; });
    auto unresolved = cancelled.next();
    require(cancelled.return_().get().done && returned == 1, "return callback");
    require(unresolved.wait_for(0s) == std::future_status::timeout, "return leaves pending read untouched");
    cancelled.enqueue(1);
    require(unresolved.get().value == 1, "pending resolver survives return");
    std::cout << "Stream lifecycle contracts passed\n";
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

#pragma once
#include <iostream>
#include <stdexcept>
#include <string>

namespace cloud_check {
inline int failures = 0;
inline void check(bool condition, const std::string& what) {
  if (!condition) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}
inline void same(const std::string& actual, const std::string& expected, const std::string& what) {
  if (actual != expected) { ++failures; std::cerr << "FAIL: " << what << "\n  expected: " << expected << "\n  actual:   " << actual << '\n'; }
}
template <class F> void throwsContaining(F&& body, const std::string& needle, const std::string& what) {
  try { body(); }
  catch (const std::exception& error) {
    if (std::string(error.what()).find(needle) == std::string::npos) {
      ++failures;
      std::cerr << "FAIL: " << what << " threw without \"" << needle << "\": " << error.what() << '\n';
    }
    return;
  }
  ++failures;
  std::cerr << "FAIL: " << what << " did not throw\n";
}
inline int finish(const std::string& name) {
  if (failures) { std::cerr << name << ": " << failures << " failure(s)\n"; return 1; }
  std::cout << name << " passed\n";
  return 0;
}
} // namespace cloud_check

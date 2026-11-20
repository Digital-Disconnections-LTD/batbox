#pragma once

#include <functional>
#include <optional>
#include <string>

namespace batbox::interactive::cards {

struct PoetryEnvironment {
  bool selected = false;
  std::function<void(const std::string&)> logError;
};

enum class PoetryBackground {Selected, User};
struct PoetryLeaf {
  std::string displayText;
  PoetryBackground background = PoetryBackground::User;
  bool addMargin = false;
  int paddingRight = 1;
};

std::string poetryDisplayText(const std::string& text);
std::optional<PoetryLeaf> poetryLeaf(const std::string& text,
                                     bool addMargin,
                                     const PoetryEnvironment& environment);
} // namespace batbox::interactive::cards

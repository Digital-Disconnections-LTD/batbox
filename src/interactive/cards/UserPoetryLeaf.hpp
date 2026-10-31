#pragma once

#include <functional>
#include <optional>
#include <string>

namespace batbox::interactive::cards {

struct PoetryEnvironment {
  bool briefFeatureEnabled = false;
  bool isBriefOnly = false;
  bool isTranscriptMode = false;
  bool viewingBatMission = false;
  bool briefEnvironmentEnabled = false;
  bool selected = false;
  std::function<bool()> briefModeActive;
  std::function<bool()> userMessageOptIn;
  std::function<bool()> briefPreference;
  std::function<void(const std::string&)> logError;
};

enum class PoetryBackground {Selected, User, None};
struct PoetryLeaf {
  std::string displayText;
  bool briefLayout = false;
  std::optional<std::string> timestamp;
  PoetryBackground background = PoetryBackground::User;
  bool addMargin = false;
  int paddingRight = 1;
};

std::string poetryDisplayText(const std::string& text);
std::optional<PoetryLeaf> poetryLeaf(const std::string& text,
                                     const std::optional<std::string>& timestamp,
                                     bool addMargin,
                                     const PoetryEnvironment& environment);
} // namespace batbox::interactive::cards

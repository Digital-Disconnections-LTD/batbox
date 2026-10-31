#pragma once
#include <functional>
#include <string>
#include <vector>

namespace batbox::application_skills {
struct PoetryShellMatch {
  std::string pattern,command;
};
using PoetryShellRun=std::function<std::string(const std::string& command,
  const std::string& pattern)>;

// Block matches precede inline matches.
std::vector<PoetryShellMatch> parsePoetryShellMatches(const std::string& text);
std::string formatPoetryShellOutput(const std::string& stdoutText,
  const std::string& stderrText);
std::string expandPoetryShell(const std::string& text,
  const std::string& slashCommandName,const PoetryShellRun& run);
}

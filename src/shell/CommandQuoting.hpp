#pragma once
#include <string>

namespace batbox::shell_command {
bool containsHeredocForQuoting(const std::string& command);
bool containsMultilineString(const std::string& command);
std::string quoteShellCommand(const std::string& command,bool addStdinRedirect=true);
bool hasStdinRedirect(const std::string& command);
bool shouldAddStdinRedirect(const std::string& command);
std::string rewriteWindowsNullRedirect(const std::string& command);
std::string formatShellPrefixCommand(const std::string& prefix,const std::string& command);
} // namespace batbox::shell_command

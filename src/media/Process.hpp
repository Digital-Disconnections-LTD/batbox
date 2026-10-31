#pragma once
#include <string>
#include <vector>
namespace batbox::media::indigenous {
struct ProcessResult {int code=-1;std::string stdoutText,stderrText;};
ProcessResult runProcess(const std::string& program,const std::vector<std::string>& args,int timeoutMs);
} // namespace batbox::media::indigenous

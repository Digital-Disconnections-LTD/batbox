#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <map>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>
#if defined(_WIN32) && defined(stdin)
// Windows CRT exposes stdin as a macro; ExecOptions uses that field name.
#undef stdin
#endif

namespace batbox::process {
struct ExecOptions {
  std::stop_token abortSignal;
  std::optional<double> timeout;
  std::optional<bool> preserveOutputOnError,useCwd;
  std::optional<double> maxBuffer;
  std::optional<std::string> cwd,stdin,input;
  std::optional<std::map<std::string,std::string>> env;
  std::optional<Json> shell;
  // Indigenous cmd.exe shell bindings supply their own command-line quoting.
  bool windowsVerbatimArguments=false;
};
struct ExecaRequest {
  std::string file;
  std::vector<std::string> args;
  ExecOptions options;
};
struct ExecaResult {
  bool failed=false;
  std::optional<int> exitCode;
  std::string stdoutText,stderrText;
  std::optional<std::string> shortMessage,signal;
};
struct ExecResult {
  std::string stdoutText,stderrText;
  int code=0;
  std::optional<std::string> error;
  Json toJson()const;
};
struct ExecImports {
  std::function<event_loop::Deferred<ExecaResult>(ExecaRequest)> execa;
  std::function<std::string()> getCwd;
  std::function<void(std::exception_ptr)> logError;
};
std::string getErrorMessage(const ExecaResult& result,int errorCode);
// Nullopt means the entire options argument was omitted. An explicit empty
// options object has different defaults in this wrapper.
event_loop::Deferred<ExecResult> execFileNoThrow(event_loop::Loop& loop,
  const std::string& file,const std::vector<std::string>& args,
  std::optional<ExecOptions> options,const ExecImports& imports);
event_loop::Deferred<ExecResult> execFileNoThrowWithCwd(event_loop::Loop& loop,
  const std::string& file,const std::vector<std::string>& args,
  std::optional<ExecOptions> options,const ExecImports& imports);
// Indigenous argv binding for execa request shape. Execa-specific diagnostics,
// implicit Windows .bat/.cmd dispatch and shell option remain separate work.
std::function<event_loop::Deferred<ExecaResult>(ExecaRequest)> indigenousExeca(
  event_loop::Loop& loop);
} // namespace batbox::process

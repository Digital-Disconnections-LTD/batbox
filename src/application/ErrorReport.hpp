#pragma once
#include <exception>
#include <string>
#include <string_view>

namespace batbox::application {

// An error that reached the host's report path, described for two readers:
// the status row (one short line) and native.log (everything we know).
struct ReportedError {
  std::string what,type;
  // Library and programming errors (std::out_of_range from substr, JSON type
  // errors, bare tokens such as "basic_string") mean nothing to the user.
  bool internal=false;
};

ReportedError describeError(std::exception_ptr error);
// "Error: …" for the status row; empty when there is nothing to report.
std::string statusErrorText(const ReportedError& error,std::string_view origin);
// The native.log line: origin, exception type and the full what().
std::string errorLogLine(const ReportedError& error,std::string_view origin);
// Status text is a single row: control characters and line breaks become
// spaces and runs of spaces collapse.
std::string statusLine(std::string_view text);

} // namespace batbox::application

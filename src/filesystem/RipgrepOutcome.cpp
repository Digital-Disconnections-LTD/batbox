#include "filesystem/RipgrepProcess.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::filesystem {
namespace {
constexpr std::size_t maxReportedErrorLines=12;
constexpr std::size_t maxReportedErrorBytes=2000;
// ripgrep reports a file or directory it could not read with the OS error
// code; anything else (a regex or glob parse error, a bad flag) ends the run.
bool pathError(std::string_view line){
  return line.find("(os error ")!=std::string_view::npos;
}
struct ErrorSummary {std::string text;bool onlyPathErrors=true;};
ErrorSummary summarize(const std::string& err){
  ErrorSummary summary;
  std::size_t start=0,lines=0;
  bool omitted=false;
  while(start<err.size()){
    const auto end=err.find('\n',start);
    auto line=std::string_view(err).substr(start,end==std::string::npos?std::string::npos:end-start);
    if(!line.empty()&&line.back()=='\r')line.remove_suffix(1);
    start=end==std::string::npos?err.size():end+1;
    if(line.find_first_not_of(" \t")==std::string_view::npos)continue;
    // Every line is classified, including those past the reporting limit.
    if(!pathError(line))summary.onlyPathErrors=false;
    if(omitted||lines==maxReportedErrorLines||
       summary.text.size()+line.size()>maxReportedErrorBytes){omitted=true;continue;}
    if(!summary.text.empty())summary.text+='\n';
    summary.text+=line;
    ++lines;
  }
  if(omitted)summary.text+="\n...";
  return summary;
}
}
std::vector<std::string> parseRipgrepLines(const std::string& output,bool dropLast){
  const auto trimmed=value::trimView(output);
  std::vector<std::string> lines;
  std::size_t start=0;
  while(start<=trimmed.size()){
    const auto end=trimmed.find('\n',start);
    auto line=trimmed.substr(start,end==std::string_view::npos?std::string_view::npos:end-start);
    if(!line.empty()&&line.back()=='\r')line.remove_suffix(1);
    if(!line.empty())lines.emplace_back(line);
    if(end==std::string_view::npos)break;
    start=end+1;
  }
  if(dropLast&&!lines.empty())lines.pop_back();
  return lines;
}
RipgrepLines finishRipgrep(const RipgrepOutcome& outcome,const RipgrepConfig& config){
  const bool stopped=outcome.timedOut||outcome.aborted;
  RipgrepLines result(parseRipgrepLines(outcome.out,stopped||outcome.overflow));
  if(stopped&&result.lines.empty())
    throw RipgrepTimeoutError("Ripgrep search timed out after "+
      std::to_string(config.timeoutDisplaySeconds)+
      " seconds. The search may have matched files but did not complete in time. Try searching a more specific path or pattern.",
      {});
  if(outcome.aborted){
    result.incomplete="Results are incomplete: the search was stopped before it finished.";
    return result;
  }
  if(outcome.timedOut){
    result.incomplete="Results are incomplete: ripgrep timed out after "+
      std::to_string(config.timeoutDisplaySeconds)+
      " seconds. Search a narrower path or pattern for the full list.";
    return result;
  }
  if(outcome.overflow){
    result.incomplete="Results are incomplete: ripgrep output passed the "+
      std::to_string(config.maxBufferBytes/1000000)+
      " MB limit. Search a narrower path or pattern for the full list.";
    return result;
  }
  const auto errors=summarize(outcome.err);
  if(result.lines.empty()&&(errors.text.empty()||!errors.onlyPathErrors))
    throw std::runtime_error(errors.text.empty()?
      "ripgrep failed with exit code "+std::to_string(outcome.exitCode)+" and no message.":
      "ripgrep could not run this search:\n"+errors.text);
  result.incomplete=errors.text.empty()?
    "Results may be incomplete: ripgrep exited with code "+
      std::to_string(outcome.exitCode)+" before finishing.":
    "Some paths could not be searched:\n"+errors.text;
  return result;
}
} // namespace batbox::filesystem

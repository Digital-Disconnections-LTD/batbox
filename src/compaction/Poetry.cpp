#include "compaction/Poetry.hpp"
#include "compaction/poetryEntries/Templates.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::compaction {
namespace {
std::string build(std::string_view text,const std::optional<std::string>& instructions) {
  std::string result(poetryEntries::preamble());
  result+=text;
  if (instructions && !value::trim(*instructions).empty()) result+="\n\nAdditional Instructions:\n"+*instructions;
  result+=poetryEntries::trailer();
  return result;
}
std::string replacement(const std::string& input,std::size_t start,std::size_t length,const std::string& text) {
  std::string result;
  for (std::size_t i=0;i<text.size();++i) {
    if (text[i]=='$' && i+1<text.size()) {
      switch (text[i+1]) {
        case '$':result+='$';++i;continue;
        case '&':result+=input.substr(start,length);++i;continue;
        case '`':result+=input.substr(0,start);++i;continue;
        case '\'':result+=input.substr(start+length);++i;continue;
        default:break; // The replacement regex has no capture groups.
      }
    }
    result+=text[i];
  }
  return result;
}
}
std::string getCompactPoetry(const std::optional<std::string>& instructions) {return build(poetryEntries::base(),instructions);}
std::string getPartialCompactPoetry(const std::optional<std::string>& instructions,const std::string& direction) {
  return build(direction=="up_to" ? poetryEntries::prefix() : poetryEntries::partial(),instructions);
}
std::string formatCompactSummary(const std::string& summary) {
  auto formatted=summary;
  const auto begin=formatted.find("<analysis>");
  if (begin!=std::string::npos) {
    const auto end=formatted.find("</analysis>",begin+10);
    if (end!=std::string::npos) formatted.erase(begin,end+11-begin);
  }
  const auto start=formatted.find("<summary>");
  if (start!=std::string::npos) {
    const auto end=formatted.find("</summary>",start+9);
    if (end!=std::string::npos) {
      auto text="Summary:\n"+value::trim(formatted.substr(start+9,end-start-9));
      formatted.replace(start,end+10-start,replacement(formatted,start,end+10-start,text));
    }
  }
  std::string result;
  unsigned newlines=0;
  for (const auto c:formatted) {
    newlines=c=='\n' ? newlines+1 : 0;
    if (newlines<=2) result+=c;
  }
  return value::trim(result);
}
std::string getCompactUserSummaryMessage(const std::string& summary,bool suppress,
  const std::optional<std::string>& transcript,bool preserved,PoetryServices& services) {
  auto result=std::string("This session is being continued from a previous conversation that ran out of context. "
    "The summary below covers the earlier portion of the conversation.\n\n")+formatCompactSummary(summary);
  if (transcript && !transcript->empty())
    result+="\n\nIf you need specific details from before compaction (like exact code snippets, error messages, "
      "or content you generated), read the full transcript at: "+*transcript;
  if (preserved) result+="\n\nRecent messages are preserved verbatim.";
  if (suppress) {
    result+="\nContinue the conversation from where it left off without asking the user any further questions. "
      "Resume directly — do not acknowledge the summary, do not recap what was happening, do not preface with "
      "\"I'll continue\" or similar. Pick up the last task as if the break never happened.";
    if ((services.feature("PROACTIVE") || services.feature("KAIROS")) &&
        services.isProactiveActive && services.isProactiveActive())
      result+="\n\nYou are running in autonomous/proactive mode. This is NOT a first wake-up — you were already "
        "working autonomously before compaction. Continue your work loop: pick up where you left off based on "
        "the summary above. Do not greet the user or ask what to work on.";
  }
  return result;
}
}

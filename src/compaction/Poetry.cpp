#include "compaction/Poetry.hpp"
#include "compaction/poetryEntries/Templates.hpp"
#include "runtime/JsonValue.hpp"
#include "services/compact/autoCompact.hpp"
#include <algorithm>
#include <cmath>

namespace batbox::compaction {
namespace {
std::string whole(double tokens) {return std::to_string(static_cast<long long>(tokens));}
// Asks for three quarters of the room kept for the summary, so a reply that
// runs a little long still ends before the cap.
std::string budget(double outputTokens) {
  const double room=std::min(outputTokens,MAX_OUTPUT_TOKENS_FOR_SUMMARY);
  const double target=std::max(1000.0,std::floor(room*0.75/100)*100);
  return "\n\nLength: your reply is cut off after "+whole(outputTokens)+" tokens. Keep the whole reply, "
    "the <analysis> block included, under about "+whole(target)+" tokens (about "+whole(std::floor(target*0.75/100)*100)+
    " words): keep <analysis> short, and in <summary> quote only the code needed to continue the work.";
}
std::string build(std::string_view text,const std::optional<std::string>& instructions,double outputTokens) {
  std::string result(poetryEntries::preamble());
  result+=text;
  if (outputTokens>0) result+=budget(outputTokens);
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
std::string getCompactPoetry(const std::optional<std::string>& instructions,double outputTokens) {
  return build(poetryEntries::base(),instructions,outputTokens);
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
    } else {
      // A summary cut at the output cap has no closing tag.
      formatted.replace(start,std::string::npos,"Summary:\n"+value::trim(formatted.substr(start+9)));
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
  const std::optional<std::string>& transcript,double cutAtTokens) {
  auto result=std::string("This session is being continued from a previous conversation that ran out of context. "
    "The summary below covers the earlier portion of the conversation.\n\n")+formatCompactSummary(summary);
  if (cutAtTokens>0) {
    result+="\n\nThis summary was cut short: it reached the "+whole(cutAtTokens)+
      "-token output limit before it was finished, so its last sections, including the most recent work, may be "
      "missing. ";
    result+=transcript && !transcript->empty() ?
      "The full transcript is at: "+*transcript+" — read its end to recover what the summary does not cover." :
      std::string("No transcript file is available for this session.");
  } else if (transcript && !transcript->empty())
    result+="\n\nIf you need specific details from before compaction (like exact code snippets, error messages, "
      "or content you generated), read the full transcript at: "+*transcript;
  if (suppress) {
    result+="\nContinue the conversation from where it left off without asking the user any further questions. "
      "Resume directly — do not acknowledge the summary, do not recap what was happening, do not preface with "
      "\"I'll continue\" or similar. Pick up the last task as if the break never happened.";
  }
  return result;
}
}

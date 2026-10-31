#pragma once
#include <functional>
#include <optional>
#include <string>
namespace batbox::compaction {
struct PoetryServices {
  std::function<bool(const std::string&)> feature;
  std::function<bool()> isProactiveActive;
};
std::string getCompactPoetry(const std::optional<std::string>& instructions={});
std::string getPartialCompactPoetry(const std::optional<std::string>& instructions={},
                                   const std::string& direction="from");
std::string formatCompactSummary(const std::string& summary);
std::string getCompactUserSummaryMessage(const std::string& summary,bool suppressFollowUpQuestions,
  const std::optional<std::string>& transcriptPath,bool recentMessagesPreserved,PoetryServices& services);
}

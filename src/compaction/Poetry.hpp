#pragma once
#include <optional>
#include <string>
namespace batbox::compaction {
std::string getCompactPoetry(const std::optional<std::string>& instructions={});
std::string formatCompactSummary(const std::string& summary);
std::string getCompactUserSummaryMessage(const std::string& summary,bool suppressFollowUpQuestions,
  const std::optional<std::string>& transcriptPath);
}

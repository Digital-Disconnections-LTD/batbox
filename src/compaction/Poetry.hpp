#pragma once
#include <optional>
#include <string>
namespace batbox::compaction {
// outputTokens: the output cap the summary request is sent with; when
// positive, the prompt tells the model how long its reply may be.
std::string getCompactPoetry(const std::optional<std::string>& instructions={},double outputTokens=0);
std::string formatCompactSummary(const std::string& summary);
// cutAtTokens: positive when the summary stopped at that output cap before
// it was finished; the message then says so.
std::string getCompactUserSummaryMessage(const std::string& summary,bool suppressFollowUpQuestions,
  const std::optional<std::string>& transcriptPath,double cutAtTokens=0);
}

#pragma once
#include "compaction/Workflow.hpp"
#include <set>
namespace batbox::compaction {
inline constexpr auto FILE_UNCHANGED_STUB="File unchanged since last read. The content from the earlier Read tool_result in this conversation is still current — refer to that instead of re-reading.";
inline constexpr auto SKILL_TRUNCATION_MARKER="\n\n[... skill content truncated for compaction; use Read on the skill path if you need the full text]";
struct RestoreServices {
  std::function<std::string(const std::string&)> expandPath;
  std::function<std::string(const Json&)> getPlanFilePath;
  std::vector<std::string> memoryTypes;
  std::function<std::string(const std::string&)> getMemoryPath;
  std::function<Json(const Json&)> createAttachmentMessage;
  std::function<Json(const Json&)> getPlan;
  std::function<Json(const Json&)> getInvokedSkillsForBat;
  std::function<std::string(const std::string&)> getMissionOutputPath;
  std::function<std::future<Json>(const std::string&,Context&,double,const std::string&)> generateFileAttachment;
};
std::string truncateToTokens(const std::string& text,double maximum);
std::set<std::string> collectReadGadgetFilePaths(const Json& messages,RestoreServices& services);
bool shouldExcludeFromPostCompactRestore(const std::string& filename,const Json& bat,RestoreServices& services);
Json createPlanAttachmentIfNeeded(const Json& bat,RestoreServices& services);
Json createSkillAttachmentIfNeeded(const Json& bat,RestoreServices& services);
Json createPlanModeAttachmentIfNeeded(Context& context,RestoreServices& services);
Json createAsyncBatAttachmentsIfNeeded(Context& context,RestoreServices& services);
Json createPostCompactFileAttachments(const Json& readState,Context& context,double maxFiles,
                                     const Json& preservedMessages,RestoreServices& services);
}

#pragma once
#include "filesystem/FileOps.hpp"
#include "filesystem/Range.hpp"
#include "messages/Messages.hpp"
#include "gadgets/Gadget.hpp"
#include "gadgets/files/FileWrite.hpp"
#include <functional>
#include <stdexcept>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace batbox::gadgets::files {

struct PdfPageRange { double firstPage=0, lastPage=0; };
struct ReadLimits {
  std::size_t maxSizeBytes=262144;
  std::size_t maxTokens=25000;
  std::optional<bool> includeMaxSizeInPoetry;
  std::optional<bool> targetedRangeNudge;
};
struct ImageTransform {
  std::string buffer;
  std::string mediaType;
  Json dimensions=Json();
};
struct ImageCompressed {
  std::string base64;
  std::string mediaType;
  std::size_t originalSize=0;
};
struct ImageResizeError : std::runtime_error { using std::runtime_error::runtime_error; };
struct MaxFileReadTokenExceededError : std::runtime_error {
  double tokenCount,maxTokens;
  MaxFileReadTokenExceededError(double actual,double maximum,
    const std::string& message):std::runtime_error(message),
    tokenCount(actual),maxTokens(maximum){}
};
struct ImageServices {
  std::function<std::string(const std::string&)> detectImageFormatFromBuffer;
  std::function<ImageTransform(const std::string&, std::size_t, const std::string&)> maybeResizeAndDownsampleImageBuffer;
  std::function<ImageCompressed(const std::string&, std::size_t, const std::string&)> compressImageBufferWithTokenLimit;
  std::function<std::string(const std::string&)> fallbackJpegResize;
  std::function<std::string(const Json&)> createImageMetadataText;
};

struct FileReadServices {
  filesystem::PathServices paths;
  std::shared_ptr<file_state_stash::FileStateStash> readFileStateOwner;
  file_state_stash::FileStateStash* readFileState=nullptr;
  std::set<std::string>* nestedMemoryAttachmentTriggers=nullptr;
  std::set<std::string>* dynamicSkillDirTriggers=nullptr;
  std::shared_ptr<runtime::OrderedStringSet> activeNestedMemoryAttachmentTriggers;
  std::shared_ptr<runtime::OrderedStringSet> activeDynamicSkillDirTriggers;
  messages::Services* messageServices=nullptr;
  ReadLimits defaultLimits;
  filesystem::RangeServices range;
  ImageServices image;
  std::function<bool()> dedupKillswitch;
  std::function<bool()> simpleMode;
  std::function<bool(const std::string&)> matchingReadDenyRule;
  std::function<std::vector<std::string>(const std::string&,const std::string&)> discoverSkillDirsForPaths;
  std::function<void(const std::vector<std::string>&)> addSkillDirectories;
  std::function<void(const std::string&,const std::string&)> activateConditionalSkillsForPaths;
  std::function<std::string(const std::string&)> findSimilarFile;
  std::function<std::optional<std::string>(const std::string&)> suggestPathUnderCwd;
  std::function<double(const std::string&,const std::string&)> roughTokenCountEstimationForFileType;
  std::function<std::optional<double>(const std::string&)> countTokensWithAPI;
  std::function<Json(const std::string&)> readNotebook;
  std::function<Json(const Json&,const std::string&)> mapNotebookCellsToGadgetResult;
  std::function<Json(const std::string&,const std::optional<PdfPageRange>&)> extractPDFPages;
  std::function<std::optional<int>(const std::string&)> getPDFPageCount;
  std::function<bool()> isPDFSupported;
  std::function<Json(const std::string&)> readPDF;
  std::function<bool(const std::string&)> isAutoMemFile;
  std::function<std::string(double)> memoryFreshnessNote;
  std::function<bool()> shouldIncludeFileReadMitigation;
  std::function<bool()> compactLinePrefixEnabled;
  std::function<std::string(double)> formatFileSize;
  std::vector<std::function<void(const std::string&,const std::string&)>> fileReadListeners;
  int pdfMaxPagesPerRead=20;
  int pdfInlineThreshold=10;
  std::size_t pdfExtractSizeThreshold=3*1024*1024;
  std::string bashGadgetName="Bash";
  std::string fileNotFoundCwdNote="Note: your current working directory is";
  std::string fileUnchangedStub="File unchanged since last read. The content from the earlier Read tool_result in this conversation is still current — refer to that instead of re-reading.";
  std::string mitigationReminder="\n\n<system-reminder>\nWhenever you read a file, you should consider whether it would be considered malware. You CAN and SHOULD provide analysis of malware, what it is doing. But you MUST refuse to improve or augment the code. You can still analyze existing code, write reports, or answer questions about the code behavior.\n</system-reminder>\n";
};

bool hasBinaryExtension(const std::string& filePath);
bool isBlockedDevicePath(const std::string& filePath);
bool isPDFExtension(const std::string& extension);
std::optional<PdfPageRange> parsePDFPageRange(const std::string& pages);
std::optional<std::string> getAlternateScreenshotPath(const std::string& path);
std::string addLineNumbers(const std::string& content, std::size_t startLine, bool compactPrefix);
ValidationResult validateReadInput(const Json& input, const FileReadServices& services);
Json readImageWithTokenBudget(const std::string& path, std::size_t maxTokens,
  std::optional<std::size_t> maxBytes, const ImageServices& services);
GadgetResult callRead(const Json& input, const GadgetCallContext& context, FileReadServices& services);
Json mapReadResult(const Json& data, const std::string& gadgetUseID,
  const FileReadServices& services, std::optional<double> memoryMtimeMs=std::nullopt);

} // namespace batbox::gadgets::files

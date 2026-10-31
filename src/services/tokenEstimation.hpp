#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
namespace batbox {
// Attachment normalization callback used before token estimation.
using AttachmentNormalizer = std::function<Json(const Json&)>;
double roughTokenCountEstimation(const std::string& content, double bytesPerToken = 4);
double bytesPerTokenForFileType(const std::string& fileExtension);
double roughTokenCountEstimationForFileType(const std::string& content, const std::string& fileExtension);
double roughTokenCountEstimationForContent(const Json& content);
double roughTokenCountEstimationForBlock(const Json& block);
double roughTokenCountEstimationForMessage(const Json& message, const AttachmentNormalizer& normalizeAttachmentForAPI);
double roughTokenCountEstimationForMessages(const Json& messages, const AttachmentNormalizer& normalizeAttachmentForAPI);
} // namespace batbox

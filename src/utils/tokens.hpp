#pragma once
#include "services/tokenEstimation.hpp"
namespace batbox {
const Json* getTokenUsage(const Json& message);
double getTokenCountFromUsage(const Json& usage);
double tokenCountFromLastAPIResponse(const Json& messages);
double finalContextTokensFromLastResponse(const Json& messages);
double messageTokenCountFromLastAPIResponse(const Json& messages);
Json getCurrentUsage(const Json& messages);
bool doesMostRecentAssistantMessageExceed200k(const Json& messages);
double getAssistantMessageContentLength(const Json& message);
double tokenCountWithEstimation(const Json& messages, const AttachmentNormalizer& normalizeAttachmentForAPI);
} // namespace batbox

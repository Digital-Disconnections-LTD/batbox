#pragma once

#include "messages/Messages.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::normalization {

struct AttachmentServices {
  messages::Services* messageServices = nullptr;
  std::function<std::string(double)> formatNumber;
  std::function<std::string(const std::string&, const std::string&)> companionIntroText;
  std::function<std::string(const std::string&, const Json&)> memoryHeader;
  std::function<std::string(double)> formatFileSize;
  std::function<std::string(const Json&)> formatDiagnosticsSummary;
  std::function<std::string(const Json&)> jsonStringify;
  std::function<std::string(const Json&)> shellQuote;
  std::function<Json(const Json&, const std::string&)> mapBashResult;
  std::function<Json(const Json&, const std::string&)> mapFileReadResult;
  std::function<std::string(const Json&)> formatPartnerMessages;
  Json outputStyleConfig = Json::object();
  std::string missionCreateGadgetName;
  std::string missionUpdateGadgetName;
  std::string missionOutputGadgetName;
  std::string sendMessageGadgetName;
  std::string batGadgetName;
  std::string fileReadGadgetName;
  std::string bashGadgetName;
  std::string fileEditGadgetName;
  std::string fileWriteGadgetName;
  std::string exploreBatType;
  std::string planBatType;
  std::string askUserQuestionGadgetName;
  std::string globGadgetName;
  std::string grepGadgetName;
  std::string planPhase4Variant;
  Json allowedGadgets = Json();
  int planBatCount = 1;
  int exploreBatCount = 1;
  int maxLinesToRead = 2000;
  std::string verifyPlanGadgetName;
  std::string exitPlanModeGadgetName;
  std::string snipNudgeText;
  bool historySnipFeature = false;
  bool verifyPlanEnabled = false;
  bool todoV2Enabled = false;
  bool batSwarmsEnabled = false;
  bool skillSearchEnabled = false;
  bool planInterviewEnabled = false;
  bool explorePlanBatsEnabled = false;
  bool embeddedSearchGadgets = false;
};

std::string wrapInSystemReminder(const std::string& text);
Json wrapMessagesInSystemReminder(const Json& messages);
std::string wrapCommandText(const std::string& raw,const Json& origin);
Json normalizeSimpleAttachmentForAPI(const Json& attachment,const AttachmentServices& services);
std::optional<Json> normalizeDeltaAttachmentForAPI(const Json& attachment,const AttachmentServices& services);
std::optional<Json> normalizeMoreAttachmentForAPI(const Json& attachment,const AttachmentServices& services);
std::optional<Json> normalizeGadgetAttachmentForAPI(const Json& attachment,const AttachmentServices& services);
std::optional<Json> normalizeModeAttachmentForAPI(const Json& attachment,const AttachmentServices& services);

} // namespace batbox::normalization

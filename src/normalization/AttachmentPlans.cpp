#include "normalization/Attachments.hpp"
#include "normalization/AttachmentPlanText.hpp"
#include "normalization/Internal.hpp"
#include <vector>

namespace batbox::normalization {
using namespace internal;
namespace {
std::string jsField(const Json& object, const char* key) {
  if (!has(object, key)) return "undefined";
  const Json& value = object.at(key);
  if (value.is_string()) return value.get<std::string>();
  if (value.is_null()) return "null";
  return value::stringify(value);
}
Json one(const std::string& content, const AttachmentServices& services) {
  need(services.messageServices != nullptr, "AttachmentServices.messageServices");
  const Json message = messages::createUserMessage(Json{{"content", content}, {"isMeta", true}}, *services.messageServices);
  return wrapMessagesInSystemReminder(Json::array({message}));
}
std::string planFileInfo(const Json& attachment, const AttachmentServices& services, bool subBat) {
  const std::string path = jsField(attachment, "planFilePath");
  if (value::truthy(get(attachment, "planExists"))) {
    return "A plan file already exists at " + path + ". You can read it and make incremental edits using the " +
      services.fileEditGadgetName + " tool" + (subBat ? " if you need to." : ".");
  }
  return "No plan file exists yet. You should create your plan at " + path + " using the " +
    services.fileWriteGadgetName + " tool" + (subBat ? " if you need to." : ".");
}
std::string phase4(const AttachmentServices& services) {
  if (services.planPhase4Variant == "trim") return plan_text::PLAN_PHASE4_TRIM;
  if (services.planPhase4Variant == "cut") return plan_text::PLAN_PHASE4_CUT;
  if (services.planPhase4Variant == "cap") return plan_text::PLAN_PHASE4_CAP;
  return plan_text::PLAN_PHASE4_CONTROL;
}
std::string readOnlyGadgetNames(const AttachmentServices& services) {
  std::vector<std::string> gadgets = services.embeddedSearchGadgets
    ? std::vector<std::string>{services.fileReadGadgetName, "`find`", "`grep`"}
    : std::vector<std::string>{services.fileReadGadgetName, services.globGadgetName, services.grepGadgetName};
  if (services.allowedGadgets.is_array() && !services.allowedGadgets.empty() && !services.embeddedSearchGadgets) {
    std::vector<std::string> filtered;
    for (const auto& gadget : gadgets) {
      for (const auto& allowed : services.allowedGadgets) {
        if (allowed == gadget) { filtered.push_back(gadget); break; }
      }
    }
    gadgets = std::move(filtered);
  }
  std::string joined;
  for (const auto& gadget : gadgets) { if (!joined.empty()) joined += ", "; joined += gadget; }
  return joined;
}
Json planModeInstructions(const Json& attachment, const AttachmentServices& services) {
  const std::string path = jsField(attachment, "planFilePath");
  if (value::truthy(get(attachment, "isSubAgent"))) {
    return one(plan_text::getPlanModeV2SubBatInstructions({
      planFileInfo(attachment, services, true), services.askUserQuestionGadgetName,
      services.askUserQuestionGadgetName}), services);
  }
  if (get(attachment, "reminderType") == "sparse") {
    const std::string workflow = services.planInterviewEnabled
      ? "Follow iterative workflow: explore codebase, interview user, write to plan incrementally."
      : "Follow 5-phase workflow.";
    return one(plan_text::getPlanModeV2SparseInstructions({path, workflow,
      services.askUserQuestionGadgetName, services.exitPlanModeGadgetName}), services);
  }
  const std::string info = planFileInfo(attachment, services, false);
  if (services.planInterviewEnabled) {
    const std::string explore = services.explorePlanBatsEnabled
      ? plan_text::planExploreBatText({services.exploreBatType}) : "";
    return one(plan_text::getPlanModeInterviewInstructions({
      info, readOnlyGadgetNames(services), explore,
      services.askUserQuestionGadgetName, services.askUserQuestionGadgetName,
      services.exitPlanModeGadgetName, services.askUserQuestionGadgetName,
      services.exitPlanModeGadgetName, services.exitPlanModeGadgetName}), services);
  }
  const std::string multiple = services.planBatCount > 1
    ? plan_text::planMultipleBatsText({std::to_string(services.planBatCount)}) : "";
  return one(plan_text::getPlanModeV2Instructions({
    info, services.exploreBatType, std::to_string(services.exploreBatCount),
    services.exploreBatType, std::to_string(services.exploreBatCount),
    services.planBatType, std::to_string(services.planBatCount), multiple,
    services.askUserQuestionGadgetName, phase4(services), services.exitPlanModeGadgetName,
    services.exitPlanModeGadgetName, services.askUserQuestionGadgetName,
    services.exitPlanModeGadgetName, services.askUserQuestionGadgetName,
    services.exitPlanModeGadgetName, services.exitPlanModeGadgetName,
    services.askUserQuestionGadgetName}), services);
}
Json autoModeInstructions(const Json& attachment, const AttachmentServices& services) {
  return get(attachment, "reminderType") == "sparse"
    ? one(plan_text::getAutoModeSparseInstructions({}), services)
    : one(plan_text::getAutoModeFullInstructions({}), services);
}
} // namespace

std::optional<Json> normalizeModeAttachmentForAPI(const Json& attachment, const AttachmentServices& services) {
  const Json& type = get(attachment, "type");
  if (type == "plan_mode") return planModeInstructions(attachment, services);
  if (type == "auto_mode") return autoModeInstructions(attachment, services);
  return std::nullopt;
}

} // namespace batbox::normalization

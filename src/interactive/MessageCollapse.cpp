#include "interactive/MessageCollapse.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive {
namespace {
const Json* member(const Json& object, const char* key) {
  if (!object.is_object()) return nullptr;
  const auto found = object.find(key);
  return found == object.end() ? nullptr : &*found;
}
std::string value(const Json& object, const char* key) {
  const auto* found = member(object, key);
  return found && found->is_string() ? found->get<std::string>() : "";
}
bool partnerShutdown(const RenderItem& item) {
  if (!item.message || value(*item.message, "type") != "attachment") return false;
  const auto* attachment = member(*item.message, "attachment");
  return attachment && value(*attachment, "type") == "task_status" &&
      value(*attachment, "taskType") == "in_process_teammate" &&
      value(*attachment, "status") == "completed";
}
bool labeledHookSummary(const RenderItem& item) {
  return item.message && value(*item.message, "type") == "system" &&
      value(*item.message, "subtype") == "stop_hook_summary" &&
      item.message->contains("hookLabel");
}
void appendArray(Json& into, const Json& message, const char* field) {
  const auto* array = member(message, field);
  if (!array || !array->is_array()) throw std::runtime_error(std::string(field) + " array required");
  for (const auto& entry : *array) into.push_back(entry);
}
} // namespace

RenderItemsRef collapsePartnerShutdowns(const RenderItemsRef& messages) {
  if (!messages) throw std::invalid_argument("Message list required");
  auto result = std::make_shared<RenderItems>();
  for (std::size_t i = 0; i < messages->size();) {
    const auto& item = (*messages)[i];
    if (!partnerShutdown(item)) { result->push_back(item); ++i; continue; }
    const auto start = i;
    while (i < messages->size() && partnerShutdown((*messages)[i])) ++i;
    const auto count = i - start;
    if (count == 1) { result->push_back(item); continue; }
    Json batch{{"type", "attachment"},
               {"uuid", item.message->at("uuid")},
               {"timestamp", item.message->at("timestamp")},
               {"attachment", Json{{"type", "teammate_shutdown_batch"},
                                    {"count", count}}}};
    result->push_back({std::make_shared<Json>(std::move(batch)), {}, {}, {}});
  }
  return result;
}

RenderItemsRef collapseHookSummaries(const RenderItemsRef& messages) {
  if (!messages) throw std::invalid_argument("Message list required");
  auto result = std::make_shared<RenderItems>();
  for (std::size_t i = 0; i < messages->size();) {
    const auto& item = (*messages)[i];
    if (!labeledHookSummary(item)) { result->push_back(item); ++i; continue; }
    const auto label = item.message->at("hookLabel");
    const auto start = i;
    while (i < messages->size() && labeledHookSummary((*messages)[i]) &&
           (*messages)[i].message->at("hookLabel") == label) ++i;
    if (i == start + 1) { result->push_back(item); continue; }
    Json merged = *item.message;
    double hookCount = 0, duration = 0;
    bool prevented = false, hasOutput = false;
    Json infos = Json::array(), errors = Json::array();
    for (std::size_t j = start; j < i; ++j) {
      const auto& source = *(*messages)[j].message;
      hookCount += source.at("hookCount").get<double>();
      appendArray(infos, source, "hookInfos");
      appendArray(errors, source, "hookErrors");
      prevented = prevented || value::truthy(value::property(source, "preventedContinuation"));
      hasOutput = hasOutput || value::truthy(value::property(source, "hasOutput"));
      const auto* time = member(source, "totalDurationMs");
      duration = std::max(duration, time && !time->is_null() ? time->get<double>() : 0.0);
    }
    merged["hookCount"] = hookCount;
    merged["hookInfos"] = std::move(infos);
    merged["hookErrors"] = std::move(errors);
    merged["preventedContinuation"] = prevented;
    merged["hasOutput"] = hasOutput;
    merged["totalDurationMs"] = duration;
    result->push_back({std::make_shared<Json>(std::move(merged)), {}, {}, {}});
  }
  return result;
}
} // namespace batbox::interactive

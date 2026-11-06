#pragma once
#include "messages/Messages.hpp"
#include <stdexcept>

namespace batbox::messages::internal {
const std::string NO_CONTENT_MESSAGE = "(no content)";
const std::string SYNTHETIC_MODEL = "<synthetic>";
const std::string SYNTHETIC_GADGET_RESULT_PLACEHOLDER = "[Tool result missing due to internal error]";

inline bool has(const Json& object, const std::string& key) { return object.is_object() && object.contains(key); }
inline const Json& get(const Json& object, const std::string& key) {
  static const Json missing;
  return has(object, key) ? object.at(key) : missing;
}
inline std::string str(const Json& value) { return value.is_string() ? value.get<std::string>() : std::string(); }
inline std::string field(const Json& object, const std::string& key) { return str(get(object, key)); }
inline void copyIf(Json& target, const Json& source, const std::string& key) {
  if (has(source, key)) target[key] = source.at(key);
}
inline std::string requireUUID(Services& services) {
  if (!services.randomUUID) throw std::logic_error("randomUUID service not supplied");
  return services.randomUUID();
}
inline std::string requireNow(Services& services) {
  if (!services.nowISO) throw std::logic_error("nowISO service not supplied");
  return services.nowISO();
}
inline Json textBlock(const std::string& text) { return Json{{"type", "text"}, {"text", text}}; }
inline bool isBlock(const Json& block, const std::string& type) {
  if (!block.is_object()) return false;
  const Json& value = get(block, "type");
  return value.is_string() ? value.get_ref<const std::string&>() == type : type.empty();
}
} // namespace batbox::messages::internal

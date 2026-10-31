#pragma once
#include "normalization/Normalization.hpp"
#include <stdexcept>

namespace batbox::normalization::internal {
inline const Json& get(const Json& object, const char* key) { return value::property(object,key); }
inline bool has(const Json& object, const char* key) { return object.is_object() && object.contains(key); }
inline bool typeIs(const Json& block, const char* type) { return get(block,"type") == type; }
inline Json textBlock(const std::string& text) { return Json{{"type","text"},{"text",text}}; }
inline void need(bool available, const char* name) {
  if (!available) throw std::runtime_error(std::string("Missing normalization dependency: ")+name);
}
Json normalizeUserTextContent(const Json& content);
Json hoistGadgetResults(const Json& content);
Json joinTextAtSeam(const Json& a,const Json& b);
Json mergeUserContentBlocks(const Json& a,const Json& b,const Dependencies& deps);
Json smooshIntoGadgetResult(const Json& tr,const Json& blocks);
Json smooshSystemReminderSiblings(const Json& messages);
Json mergeAdjacentUserMessages(const Json& messages,const Dependencies& deps);
bool contentHasGadgetReference(const Json& content);
Json relocateGadgetReferenceSiblings(const Json& messages);
} // namespace batbox::normalization::internal

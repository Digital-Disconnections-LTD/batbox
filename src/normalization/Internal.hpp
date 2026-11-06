#pragma once
#include "normalization/Normalization.hpp"
#include <stdexcept>

namespace batbox::normalization::internal {
inline const Json& get(const Json& object, const char* key) { return value::property(object,key); }
inline bool has(const Json& object, const char* key) { return object.is_object() && object.contains(key); }
inline bool typeIs(const Json& block, const char* type) { return value::stringEquals(get(block,"type"),type); }
inline Json textBlock(const std::string& text) { return Json{{"type","text"},{"text",text}}; }
inline void need(bool available, const char* name) {
  if (!available) throw std::runtime_error(std::string("Missing normalization dependency: ")+name);
}
Json normalizeUserTextContent(Json content);
Json hoistGadgetResults(Json content);
Json joinTextAtSeam(Json a,const Json& b);
Json mergeUserContentBlocks(Json a,const Json& b);
Json smooshIntoGadgetResult(const Json& tr,const Json& blocks);
Json mergeAdjacentUserMessages(Json messages);
bool contentHasGadgetReference(const Json& content);
} // namespace batbox::normalization::internal

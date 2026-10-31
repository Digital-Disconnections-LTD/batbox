#pragma once
#include "execution/GadgetExecution.hpp"
#include <stdexcept>

namespace batbox::execution::internal {
inline const Json& field(const Json& object, const char* name) { return value::property(object,name); }
inline std::string stringField(const Json& object, const char* name) {
  const auto& value=field(object,name);
  return value.is_string()?value.get<std::string>():std::string{};
}
template <class T> inline void require(const T& callback,const char* name) {
  if(!callback)throw std::invalid_argument(std::string("Unbound tool execution import: ")+name);
}
inline Json createResult(const std::string& id,const Json& content,bool isError=true) {
  return Json{{"type","tool_result"},{"content",content},{"is_error",isError},{"tool_use_id",id}};
}
void checkPermissionsAndCallGadget(const gadgets::Gadget& gadget,const Json& gadgetUse,
  const Json& assistantMessage,Context& context,Services& services,
  const std::function<void(const Update&)>& yield);
} // namespace batbox::execution::internal

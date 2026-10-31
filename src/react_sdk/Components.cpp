#include "react_sdk/Components.hpp"

namespace batbox::react_sdk {
ValueRef memo(ValueRef type,ValueRef compare){
  auto record=props();
  record->set("$$typeof",symbolFor("react.memo"));
  record->set("type",std::move(type));
  record->set("compare",isUndefined(compare)?nullValue():std::move(compare));
  return object(std::move(record));
}
ValueRef forwardRef(ValueRef render){
  auto record=props();
  record->set("$$typeof",symbolFor("react.forward_ref"));
  record->set("render",std::move(render));
  return object(std::move(record));
}
ValueRef fragment(){return symbolFor("react.fragment");}
ValueRef suspense(){return symbolFor("react.suspense");}
}

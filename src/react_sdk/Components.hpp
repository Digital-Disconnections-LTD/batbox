#pragma once
#include "react_sdk/Value.hpp"

namespace batbox::react_sdk {
ValueRef memo(ValueRef type,ValueRef compare=undefined());
ValueRef forwardRef(ValueRef render);
ValueRef fragment();
ValueRef suspense();
}

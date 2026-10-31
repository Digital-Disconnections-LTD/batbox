#pragma once
#include "react_sdk/Value.hpp"

namespace batbox::react_sdk {
ValueRef createContext(ValueRef defaultValue);
ValueRef contextProvider(const ValueRef& context);
ValueRef contextConsumer(const ValueRef& context);
ValueRef createRef();
}

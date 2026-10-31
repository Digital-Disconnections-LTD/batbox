#pragma once
#include "runtime/JsonValue.hpp"

namespace batbox::settings::mdm {
struct MdmResult {
  Json settings=Json::object();
  Json errors=Json::array();
};
}

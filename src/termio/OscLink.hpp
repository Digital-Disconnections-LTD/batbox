#pragma once
#include "runtime/JsonValue.hpp"
namespace batbox::termio {
std::string osc(const Json& parts,const std::string& terminal);
}

#pragma once
#include "termio/Csi.hpp"
namespace batbox::termio {
std::string decset(double mode);
std::string decreset(double mode);
inline const std::string EFE=decset(1004),DFE=decreset(1004);
}

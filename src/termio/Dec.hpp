#pragma once
#include "termio/Csi.hpp"
namespace batbox::termio {
inline std::string decset(double mode){return csi(Json::array({"?"+value::numberToString(mode)+"h"}));}
inline std::string decreset(double mode){return csi(Json::array({"?"+value::numberToString(mode)+"l"}));}
inline const std::string EFE=decset(1004),DFE=decreset(1004);
}

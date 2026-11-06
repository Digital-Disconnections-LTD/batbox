#pragma once
#include "runtime/JsonValue.hpp"
namespace batbox::termio {
inline const std::string CSI_PREFIX="\x1b[";
bool isCSIParam(double byte);
bool isCSIIntermediate(double byte);
bool isCSIFinal(double byte);
std::string csi(const Json& args=Json::array());
inline const std::string PASTE_START=csi(Json::array({"200~"}));
inline const std::string PASTE_END=csi(Json::array({"201~"}));
inline const std::string FOCUS_IN=csi(Json::array({"I"}));
inline const std::string FOCUS_OUT=csi(Json::array({"O"}));
}

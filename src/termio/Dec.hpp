#pragma once
#include "termio/Csi.hpp"
namespace batbox::termio {
std::string decset(double mode);
std::string decreset(double mode);
inline const std::string BSU=decset(2026),ESU=decreset(2026);
inline const std::string EBP=decset(2004),DBP=decreset(2004);
inline const std::string EFE=decset(1004),DFE=decreset(1004);
inline const std::string SHOW_CURSOR=decset(25),HIDE_CURSOR=decreset(25);
inline const std::string ENTER_ALT_SCREEN=decset(1049),EXIT_ALT_SCREEN=decreset(1049);
inline const std::string ENABLE_MOUSE_TRACKING=decset(1000)+decset(1002)+decset(1003)+decset(1006);
inline const std::string DISABLE_MOUSE_TRACKING=decreset(1006)+decreset(1003)+decreset(1002)+decreset(1000);
}

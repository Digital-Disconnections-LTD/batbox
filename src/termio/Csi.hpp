#pragma once
#include "runtime/JsonValue.hpp"
namespace batbox::termio {
inline const std::string CSI_PREFIX="\x1b[";
bool isCSIParam(double byte);
bool isCSIIntermediate(double byte);
bool isCSIFinal(double byte);
std::string csi(const Json& args=Json::array());
std::string cursorUp(double n=1);
std::string cursorDown(double n=1);
std::string cursorForward(double n=1);
std::string cursorBack(double n=1);
std::string cursorTo(double column);
std::string cursorPosition(double row,double column);
std::string cursorMove(double x,double y);
std::string eraseToEndOfLine();
std::string eraseToStartOfLine();
std::string eraseLine();
std::string eraseToEndOfScreen();
std::string eraseToStartOfScreen();
std::string eraseScreen();
std::string eraseLines(double n);
std::string scrollUp(double n=1);
std::string scrollDown(double n=1);
std::string setScrollRegion(double top,double bottom);
inline const std::string CURSOR_LEFT=csi(Json::array({"G"}));
inline const std::string CURSOR_HOME=csi(Json::array({"H"}));
inline const std::string CURSOR_SAVE=csi(Json::array({"s"}));
inline const std::string CURSOR_RESTORE=csi(Json::array({"u"}));
inline const std::string ERASE_LINE=csi(Json::array({2,"K"}));
inline const std::string ERASE_SCREEN=csi(Json::array({2,"J"}));
inline const std::string ERASE_SCROLLBACK=csi(Json::array({3,"J"}));
inline const std::string RESET_SCROLL_REGION=csi(Json::array({"r"}));
inline const std::string PASTE_START=csi(Json::array({"200~"}));
inline const std::string PASTE_END=csi(Json::array({"201~"}));
inline const std::string FOCUS_IN=csi(Json::array({"I"}));
inline const std::string FOCUS_OUT=csi(Json::array({"O"}));
inline const std::string ENABLE_KITTY_KEYBOARD=csi(Json::array({">1u"}));
inline const std::string DISABLE_KITTY_KEYBOARD=csi(Json::array({"<u"}));
inline const std::string ENABLE_MODIFY_OTHER_KEYS=csi(Json::array({">4;2m"}));
inline const std::string DISABLE_MODIFY_OTHER_KEYS=csi(Json::array({">4m"}));
}

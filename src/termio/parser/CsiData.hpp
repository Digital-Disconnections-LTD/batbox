#pragma once
#include <string>
#include <utility>
namespace batbox::termio::parser::data {
constexpr int CURSOR_VISIBLE=25,ALT_SCREEN=47,ALT_SCREEN_CLEAR=1049;
constexpr int MOUSE_NORMAL=1000,MOUSE_BUTTON=1002,MOUSE_ANY=1003;
constexpr int FOCUS_EVENTS=1004,BRACKETED_PASTE=2004;
std::string eraseDisplay(double index);
std::string eraseLine(double index);
std::pair<std::string,bool> cursorStyle(double index);
}

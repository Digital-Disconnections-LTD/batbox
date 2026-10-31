#pragma once
#include <string>

namespace batbox::diagnostics {
double parseLogISOString(const std::string& text);
std::string logDateISOString(double milliseconds);
std::string dateToFilename(double milliseconds);
}

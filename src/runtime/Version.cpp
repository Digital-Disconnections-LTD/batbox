#include "runtime/Version.hpp"

#ifndef BATBOX_VERSION
#error "BATBOX_VERSION must be defined for runtime/Version.cpp"
#endif

namespace batbox::runtime {
const char* batboxVersion(){return BATBOX_VERSION;}
std::string userAgent(){return std::string("batbox/")+BATBOX_VERSION;}
} // namespace batbox::runtime

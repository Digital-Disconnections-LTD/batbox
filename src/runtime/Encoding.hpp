#pragma once
#include <string>
#include <string_view>
namespace batbox::value {
// Node UTF-8 string encoding replaces unpaired UTF-16 surrogates. Internal
// strings retain them until an actual Buffer/filesystem/process boundary.
std::string encodeUtf8(std::string_view text);
}

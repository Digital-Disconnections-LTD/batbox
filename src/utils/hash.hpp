#pragma once
#include <cstdint>
#include <string_view>
namespace batbox::utils {
std::int32_t djb2Hash(std::string_view text);
}

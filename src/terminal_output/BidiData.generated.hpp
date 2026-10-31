#pragma once
#include <cstdint>
#include <array>
namespace batbox::terminal_output::bidi_data {
struct Range {std::uint32_t start,end,type;};
struct Pair {std::uint32_t from,to;};
inline constexpr std::uint32_t L=1u;
inline constexpr std::uint32_t R=2u;
inline constexpr std::uint32_t EN=4u;
inline constexpr std::uint32_t ES=8u;
inline constexpr std::uint32_t ET=16u;
inline constexpr std::uint32_t AN=32u;
inline constexpr std::uint32_t CS=64u;
inline constexpr std::uint32_t B=128u;
inline constexpr std::uint32_t S=256u;
inline constexpr std::uint32_t WS=512u;
inline constexpr std::uint32_t ON=1024u;
inline constexpr std::uint32_t BN=2048u;
inline constexpr std::uint32_t NSM=4096u;
inline constexpr std::uint32_t AL=8192u;
inline constexpr std::uint32_t LRO=16384u;
inline constexpr std::uint32_t RLO=32768u;
inline constexpr std::uint32_t LRE=65536u;
inline constexpr std::uint32_t RLE=131072u;
inline constexpr std::uint32_t PDF=262144u;
inline constexpr std::uint32_t LRI=524288u;
inline constexpr std::uint32_t RLI=1048576u;
inline constexpr std::uint32_t FSI=2097152u;
inline constexpr std::uint32_t PDI=4194304u;
inline constexpr std::array<Range,699> charTypes{{
#include "terminal_output/data/charTypes-00.inc"
#include "terminal_output/data/charTypes-01.inc"
#include "terminal_output/data/charTypes-02.inc"
}};
inline constexpr std::array<Pair,60> openToClose{{
#include "terminal_output/data/openToClose-00.inc"
}};
inline constexpr std::array<Pair,60> closeToOpen{{
#include "terminal_output/data/closeToOpen-00.inc"
}};
inline constexpr std::array<Pair,22> canonical{{
#include "terminal_output/data/canonical-00.inc"
}};
} // namespace batbox::terminal_output::bidi_data

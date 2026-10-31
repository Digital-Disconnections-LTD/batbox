#include "terminal_ftxui/StyleProjection.hpp"
#include "ftxui/screen/color.hpp"
#include <charconv>
#include <string>
#include <string_view>
#include <vector>

namespace batbox::terminal_ftxui {
namespace {
using Color = ftxui::Color;

struct Parameters {
  std::vector<int> values;
  std::vector<char> separators;
};

Parameters parameters(std::string_view code) {
  if (!code.starts_with("\x1b[") || !code.ends_with('m'))
    throw ProjectionError("Unsupported terminal style sequence");
  code.remove_prefix(2);
  code.remove_suffix(1);
  if (code.empty()) return {{0}, {'\0'}};
  Parameters result;
  char separator = '\0';
  while (true) {
    const auto end = code.find_first_of(";:");
    const auto field = code.substr(0, end);
    result.separators.push_back(separator);
    if (field.empty()) result.values.push_back(separator == ':' ? -1 : 0);
    else {
      int value = 0;
      const auto parse = std::from_chars(field.data(), field.data() + field.size(), value);
      if (parse.ec != std::errc{} || parse.ptr != field.data() + field.size())
        throw ProjectionError("Invalid terminal style parameter");
      result.values.push_back(value);
    }
    if (end == std::string_view::npos) break;
    separator = code[end];
    code.remove_prefix(end + 1);
  }
  return result;
}

void extendedColor(const Parameters& params, std::size_t& index,
                   Color& destination) {
  const auto& values = params.values;
  if (++index >= values.size())
    throw ProjectionError("Incomplete extended terminal color");
  const int mode = values[index];
  if (mode == 5) {
    if (++index >= values.size() || values[index] < 0 || values[index] > 255)
      throw ProjectionError("Invalid indexed terminal color");
    destination = Color(static_cast<Color::Palette256>(values[index]));
    return;
  }
  if (mode == 2) {
    // Colon syntax has a colorspace parameter (empty or zero). Semicolon
    // syntax starts immediately with red, including red=0.
    if (index + 1 < values.size() && params.separators[index] == ':' &&
        (values[index + 1] == -1 || values[index + 1] == 0))
      ++index;
    if (index + 3 >= values.size())
      throw ProjectionError("Incomplete RGB terminal color");
    for (std::size_t offset = 1; offset <= 3; ++offset)
      if (values[index + offset] < 0 || values[index + offset] > 255)
        throw ProjectionError("Invalid RGB terminal color");
    destination = Color::RGB(static_cast<std::uint8_t>(values[index + 1]),
                             static_cast<std::uint8_t>(values[index + 2]),
                             static_cast<std::uint8_t>(values[index + 3]));
    index += 3;
    return;
  }
  throw ProjectionError("Unsupported extended terminal color mode");
}

void apply(const Parameters& params, ftxui::Cell& cell) {
  const auto& values = params.values;
  for (std::size_t i = 0; i < values.size(); ++i) {
    const int value = values[i];
    if (i > 0 && params.separators[i] == ':')
      throw ProjectionError("Unsupported colon terminal style group");
    switch (value) {
      case 0: {
        auto character = std::move(cell.character);
        const auto hyperlink = cell.hyperlink;
        cell = ftxui::Cell{};
        cell.character = std::move(character);
        cell.hyperlink = hyperlink;
        break;
      }
      case 1: cell.bold = true; break;
      case 2: cell.dim = true; break;
      case 3: cell.italic = true; break;
      case 4:
        if (i + 1 < values.size() && params.separators[i + 1] == ':') {
          const auto underline = values[++i];
          if (underline < 0 || underline > 2)
            throw ProjectionError("Unsupported terminal underline style");
          cell.underlined = underline == 1;
          cell.underlined_double = underline == 2;
        } else cell.underlined = true;
        break;
      case 5: cell.blink = true; break;
      case 7: cell.inverted = true; break;
      case 9: cell.strikethrough = true; break;
      case 21: cell.underlined_double = true; break;
      case 22: cell.bold = false; cell.dim = false; break;
      case 23: cell.italic = false; break;
      case 24: cell.underlined = false; cell.underlined_double = false; break;
      case 25: cell.blink = false; break;
      case 27: cell.inverted = false; break;
      case 29: cell.strikethrough = false; break;
      case 39: cell.foreground_color = Color::Default; break;
      case 49: cell.background_color = Color::Default; break;
      case 38: extendedColor(params, i, cell.foreground_color); break;
      case 48: extendedColor(params, i, cell.background_color); break;
      default:
        if (value >= 30 && value <= 37)
          cell.foreground_color = Color(static_cast<Color::Palette16>(value - 30));
        else if (value >= 40 && value <= 47)
          cell.background_color = Color(static_cast<Color::Palette16>(value - 40));
        else if (value >= 90 && value <= 97)
          cell.foreground_color = Color(static_cast<Color::Palette16>(value - 90 + 8));
        else if (value >= 100 && value <= 107)
          cell.background_color = Color(static_cast<Color::Palette16>(value - 100 + 8));
        else
          throw ProjectionError("Unsupported terminal style parameter: " +
                                std::to_string(value));
    }
  }
}
} // namespace

void projectStyle(const ansi_tokenize::Codes& codes, ftxui::Cell& cell) {
  for (const auto& code : codes) {
    if (!code) throw ProjectionError("Null terminal style");
    // Hyperlink identity is carried by the source cell, not by its style ID.
    if (code->code.starts_with("\x1b]8;")) continue;
    apply(parameters(code->code), cell);
  }
}
} // namespace batbox::terminal_ftxui

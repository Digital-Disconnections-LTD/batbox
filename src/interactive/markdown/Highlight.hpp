#pragma once

#include <string>
#include <string_view>

namespace batbox::interactive::markdown {

// SGR sequences for each token class; empty entries leave text unstyled.
struct SyntaxPalette {
  std::string keyword,string,number,comment,type,added,removed,hunk;
};

// A small lexical highlighter for the languages that show up most in coding
// Flights. It colors keywords, strings, numbers, comments and type names;
// anything it does not recognize stays plain.
bool supportsSyntaxLanguage(std::string_view language);
std::string highlightSyntax(std::string_view code,std::string_view language,
    const SyntaxPalette& palette);
// Maps a file name or extension ("src/a.cpp", "py") to a supported language,
// or an empty string.
std::string languageForPath(std::string_view path);

} // namespace batbox::interactive::markdown

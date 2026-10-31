#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace batbox::ansi_tokenize {
struct AnsiCode {
  std::string type="ansi",code,endCode;
};
using CodeRef=std::shared_ptr<AnsiCode>;
using Codes=std::vector<CodeRef>;
struct Token {
  std::string type;
  std::optional<std::string> value;
  bool fullWidth=false;
  CodeRef ansi;
  std::string controlCode;
};
using Tokens=std::vector<Token>;
} // namespace batbox::ansi_tokenize

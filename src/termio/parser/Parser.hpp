#pragma once
#include "termio/Tokenizer.hpp"
#include "termio/parser/Types.hpp"
#include <optional>
#include <string>
#include <vector>

namespace batbox::termio::parser {
std::optional<Action> parseCSI(const std::string& rawSequence);
std::optional<Action> parseEsc(const std::string& chars);
std::optional<Action> parseOSC(const std::string& content);
std::optional<Color> parseOscColor(const std::string& spec);
TextStyle applySGR(const std::string& params,const TextStyle& style);
std::string identifySequence(const std::string& sequence);
int graphemeWidth(const std::string& grapheme);
std::vector<Grapheme> segmentGraphemes(const std::string& text);

class Parser {
public:
  TextStyle style=defaultStyle();
  bool inLink=false;
  std::optional<std::string> linkUrl;
  std::vector<Action> feed(const std::string& input);
  void reset();
private:
  std::vector<Action> processToken(const termio::Token& token);
  std::vector<Action> processText(const std::string& text);
  std::vector<Action> processSequence(const std::string& sequence);
  termio::Tokenizer tokenizer_;
};
}

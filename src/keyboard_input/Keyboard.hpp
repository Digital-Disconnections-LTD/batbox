#pragma once
#include "terminal_events/ParsedKey.hpp"
#include "termio/Tokenizer.hpp"
#include "termio/Querier.hpp"
#include "runtime/JsonValue.hpp"
#include <memory>
#include <cstdint>
#include <variant>
namespace batbox::keyboard_input {
using ParsedKey=terminal_events::ParsedKey;
using ByteBuffer=std::shared_ptr<std::vector<unsigned char>>;
using Input=std::variant<Json,ByteBuffer>;
namespace decrpm_status {
inline constexpr int NOT_RECOGNIZED=0,SET=1,RESET=2,PERMANENTLY_SET=3,PERMANENTLY_RESET=4;
}
struct Modifiers {bool shift=false,meta=false,ctrl=false,super=false;};
std::uint32_t toUint32(double value);
Modifiers decodeModifier(double modifier);
std::optional<std::string> keycodeToName(double keycode);
std::optional<std::string> keyName(const std::string& code);
bool isShiftKey(const std::string& code);
bool isCtrlKey(const std::string& code);
const std::vector<std::string>& nonAlphanumericKeys();
std::string inputToString(const Input& input);
ParsedKey createPasteKey(const std::string& content);
ParsedKey createNavKey(const std::string& sequence,const std::string& name,bool ctrl);
ParsedKey parseKeypress(const std::string& sequence="");
std::optional<Json> parseTerminalResponse(const std::string& sequence);
std::vector<double> splitNumericParams(const std::string& params);
struct ParsedMouse {double button=0,col=0,row=0;std::string action,sequence;};
using KeyRef=std::shared_ptr<ParsedKey>;
using MouseRef=std::shared_ptr<ParsedMouse>;
struct ParsedResponse {std::string sequence;termio::ResponseRef response;};
using ParsedInput=std::variant<KeyRef,MouseRef,ParsedResponse>;
std::optional<ParsedMouse> parseMouseEvent(const std::string& sequence);
struct KeyParseState {
  std::string mode="NORMAL",incomplete,pasteBuffer;
  std::shared_ptr<termio::Tokenizer> tokenizer;
};
struct ParseResult {std::vector<ParsedInput> keys;KeyParseState state;};
ParseResult parseMultipleKeypresses(const KeyParseState& previous,
  const Input& input=Json(""));
Json toJson(const ParsedKey& key);
Json toJson(const ParsedInput& input);
}

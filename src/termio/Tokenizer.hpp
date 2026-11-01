#pragma once
#include <string>
#include <vector>
namespace batbox::termio {
struct Token {std::string type,value;};
enum class InputState {ground,escape,escapeIntermediate,csi,ss3,osc,dcs,apc};
struct TokenizeResult {std::vector<Token> tokens;InputState state;std::u16string buffer;};
TokenizeResult tokenize(const std::u16string& input,InputState initialState,
  const std::u16string& initialBuffer,bool flush,bool x10Mouse);
class Tokenizer {
public:
  explicit Tokenizer(bool x10Mouse=false):x10Mouse_(x10Mouse){}
  std::vector<Token> feed(const std::string& input);
  std::vector<Token> flush();
  void reset();
  std::string buffer()const;
private:
  InputState state_=InputState::ground;
  std::u16string buffer_;
  // Raw terminal reads may split one UTF-8 character across several feeds.
  std::string pendingUtf8_;
  bool x10Mouse_;
};
}

#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
namespace batbox::termio {
std::string osc(const Json& parts,const std::string& terminal);
std::string osc8Id(const std::string& url);
class OscModule {
public:
  explicit OscModule(std::function<std::string()> terminal):terminal_(std::move(terminal)),
    linkEnd_(osc(Json::array({8,"",""}),terminal_())){}
  std::string link(const std::string& url,const std::optional<Json>& params={})const;
  const std::string& linkEnd()const{return linkEnd_;}
private:
  std::function<std::string()> terminal_;
  std::string linkEnd_;
};
}

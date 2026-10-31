#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
namespace batbox::text_width {
using WrapAnsiFunction=std::function<std::string(const std::string&,double,const std::optional<Json>&)>;
using WrapAnsiRef=std::shared_ptr<WrapAnsiFunction>;
struct WrapAnsiServices {
  WrapAnsiRef wrapAnsiNpm;
  std::function<WrapAnsiRef()> resolveBunWrapAnsi;
};
class WrapAnsiModule {
public:
  explicit WrapAnsiModule(WrapAnsiServices services):wrapAnsiBun_(services.resolveBunWrapAnsi()),
    wrapAnsi_(wrapAnsiBun_?wrapAnsiBun_:std::move(services.wrapAnsiNpm)){}
  WrapAnsiRef wrapAnsi()const{return wrapAnsi_;}
private:
  WrapAnsiRef wrapAnsiBun_,wrapAnsi_;
};
}

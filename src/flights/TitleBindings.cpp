#include "flights/TitleBindings.hpp"
#include <stdexcept>
namespace batbox::flights {
TitleServices bindTitleParser(TitleServices services,
  std::shared_ptr<settings::persistence::SafeJsonParser> parser){
  if(!parser)throw std::invalid_argument("Session title JSON parser required");
  services.safeParseJson=[parser=std::move(parser)](const std::string& text){
    return parser->safeParseJSON(text);
  };
  return services;
}
}

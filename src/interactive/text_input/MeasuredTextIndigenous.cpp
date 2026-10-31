#include "interactive/text_input/MeasuredText.hpp"
#include "runtime/Unicode.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::interactive::text_input {
MetricsServices indigenousMetricsServices(
    std::shared_ptr<intl::Stash> intl,
    std::shared_ptr<text_width::StringWidth> width,
    text_width::WrapAnsiRef wrap){
  if(!intl||!width||!wrap)
    throw std::invalid_argument("Text metric services required");
  MetricsServices services;
  services.normalizeNfc=value::normalizeNFC;
  services.stringWidth=[width=std::move(width)](const std::string& text){
    return width->stringWidth(Json(text));
  };
  services.wrapAnsi=[wrap=std::move(wrap)](const std::string& text,double columns){
    return (*wrap)(text,columns,Json{{"hard",true},{"trim",false}});
  };
  services.grapheme=intl->getGraphemeSegmenter();
  services.word=intl->getWordSegmenter();
  return services;
}
} // namespace batbox::interactive::text_input

#include "terminal_output/LineRender.hpp"
#include "terminal_output/Bidi.hpp"
#include "text_width/StringWidth.hpp"
#include <stdexcept>

namespace batbox::terminal_output {
LineServices makeIndigenousLineServices(std::shared_ptr<intl::Stash> intl,
    std::shared_ptr<text_width::StringWidth> width,
    std::shared_ptr<BidiReorder> bidi){
  if(!intl||!width||!bidi)
    throw std::invalid_argument("Native line services require Intl, width and bidi");
  LineServices services;
  services.tokenize=ansi_tokenize::tokenize;
  services.styledCharsFromTokens=ansi_tokenize::styledCharsFromTokens;
  services.getGraphemeSegmenter=[intl]{return intl->getGraphemeSegmenter();};
  services.stringWidth=[width](const std::string& text){
    return width->stringWidth(text);};
  services.reorderBidi=[bidi](const ClusteredCharsRef& chars){
    return bidi->reorder(chars);};
  return services;
}
} // namespace batbox::terminal_output

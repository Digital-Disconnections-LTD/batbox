#include "interactive/navigation/MessageCopy.hpp"

namespace batbox::interactive::navigation {
void bindClipboard(CopyServices& services,termio::ClipboardModule& clipboard){
  services.setClipboard=[&clipboard](const std::string& value){
    return clipboard.setClipboard(value);
  };
}
} // namespace batbox::interactive::navigation

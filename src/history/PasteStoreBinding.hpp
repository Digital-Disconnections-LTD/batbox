#pragma once
#include "history/Writer.hpp"
#include "history/PastedContent.hpp"
#include "paste_store/PasteStore.hpp"
#include <stdexcept>

namespace batbox::history {
inline void bindPasteStore(HistoryWriterServices& writer,
    PastedContentImports& reader,paste_store::Services services){
  if(!services.loop||!writer.loop||writer.loop!=services.loop||
      (reader.loop&&reader.loop!=services.loop))
    throw std::invalid_argument("History paste store must share owner loop");
  reader.loop=services.loop;
  writer.hashPastedText=paste_store::hashPastedText;
  writer.storePastedText=[services](const std::string& hash,
      const std::string& content){
    return paste_store::storePastedText(hash,content,services);
  };
  reader.retrievePastedText=[services](const std::string& hash){
    return paste_store::retrievePastedText(hash,services);
  };
}
}

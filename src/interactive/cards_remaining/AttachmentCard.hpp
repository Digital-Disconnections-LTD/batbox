#pragma once

#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <vector>

namespace batbox::interactive::cards_remaining {
struct AttachmentSpan {
  std::string text;
  bool bold=false;
  std::optional<std::string> filePath;
};
struct AttachmentLine {
  std::vector<AttachmentSpan> spans;
  std::string colorRole;
  bool dim=true,response=true;
  int marginTop=0;
};
struct AttachmentCard {
  enum class Kind {Hidden,Lines,Memories,UserPoetry,Diagnostics};
  Kind kind=Kind::Hidden;
  Json original=Json(),selectedBackground=Json();
  bool addMargin=false,verbose=false,transcriptMode=false;
  std::vector<AttachmentLine> lines;
  std::vector<Json> memories;
  std::string poetryText;
  std::vector<int> imagePasteIDs;
};
struct AttachmentServices {
  std::function<Json()> readSelectedBackground;
  std::function<std::string(const std::string&)> displayPath;
};

AttachmentCard classifyAttachment(const Json& attachment,bool addMargin,
    bool verbose,bool transcriptMode,const AttachmentServices& services);

void classifyAttachmentMissionStatus(AttachmentCard& card);
}

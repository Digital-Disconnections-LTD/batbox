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
  std::optional<std::string> colorRole;
};
struct AttachmentLine {
  std::vector<AttachmentSpan> spans;
  std::string colorRole;
  bool dim=true,response=true;
  int marginTop=0;
};
struct MailboxItem {
  enum class Kind {MissionAssignment,PlanApproval,PartnerText};
  Kind kind=Kind::PartnerText;
  Json original=Json();
  Json parsed=Json();
  std::string text,displayName,summary,color;
};
struct AttachmentCard {
  enum class Kind {Hidden,Lines,Memories,UserPoetry,Diagnostics,Mailbox};
  Kind kind=Kind::Hidden;
  Json original=Json(),selectedBackground=Json();
  bool addMargin=false,verbose=false,transcriptMode=false;
  std::vector<AttachmentLine> lines;
  std::vector<Json> memories;
  std::string poetryText;
  std::vector<int> imagePasteIDs;
  std::vector<MailboxItem> mailbox;
};
struct AttachmentServices {
  std::function<Json()> readSelectedBackground;
  std::function<bool()> batSwarmsEnabled;
  std::function<bool()> skillSearchEnabled;
  std::function<bool()> isDemoEnvironment;
  std::function<std::string(const std::string&)> displayPath;
  std::function<bool(const std::string&)> isShutdownApproved;
  std::function<bool(const std::string&,const std::string&)> isPlanApproval;
  std::function<std::optional<std::string>(const std::string&)> formatPartnerContent;
  std::function<std::optional<Json>(const std::string&)> lookupMission;
};

AttachmentCard classifyAttachment(const Json& attachment,bool addMargin,
    bool verbose,bool transcriptMode,const AttachmentServices& services);

void classifyAttachmentMailbox(AttachmentCard& card,
    const AttachmentServices& services);
void classifyAttachmentMissionStatus(AttachmentCard& card,
    const AttachmentServices& services);
}

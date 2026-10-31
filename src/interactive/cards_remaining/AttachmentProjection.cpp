#include "interactive/cards_remaining/AttachmentProjection.hpp"
#include "runtime/Encoding.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::interactive::cards_remaining {
namespace {
void require(const auto& callback,const char* name){
  if(!callback)throw std::logic_error(std::string(name)+" import required");
}
std::string field(const Json& object,const char* key){
  if(!object.is_object())return {};
  const auto it=object.find(key);
  return it!=object.end()&&it->is_string()?it->get<std::string>():"";
}
ftxui::Element response(ftxui::Element body,ftxui::Color subtle){
  return ftxui::hbox({ftxui::text("  ⎿  ")|
    ftxui::color(subtle)|ftxui::dim,std::move(body)});
}
ftxui::Color roleColor(const std::string& role,
                      const AttachmentProjectionImports& imports){
  if(role=="error")return imports.palette.error;
  if(role=="warning")return imports.palette.warning;
  return imports.palette.text;
}
ftxui::Element lineElement(const AttachmentLine& line,
                           const AttachmentProjectionImports& imports){
  std::vector<ftxui::Element> parts;
  for(const auto& span:line.spans){
    auto element=ftxui::text(value::encodeUtf8(span.text));
    if(span.bold)element=element|ftxui::bold;
    if(span.filePath){
      require(imports.supportsHyperlinks,"terminal hyperlink support");
      if(imports.supportsHyperlinks()){
        require(imports.filePathURL,"file path URL");
        element=element|ftxui::underlined|
          ftxui::hyperlink(imports.filePathURL(*span.filePath));
      }
    }
    if(span.colorRole){
      require(imports.partnerColor,"Partner color");
      element=element|ftxui::color(imports.partnerColor(*span.colorRole));
    }
    parts.push_back(std::move(element));
  }
  auto body=ftxui::hflow(std::move(parts))|
    ftxui::color(roleColor(line.colorRole,imports));
  if(line.dim)body=body|ftxui::dim;
  if(line.response)body=response(std::move(body),imports.palette.subtle);
  if(line.marginTop)body=ftxui::vbox({ftxui::text(""),std::move(body)});
  return body;
}
ftxui::Element mailboxElement(const MailboxItem& item,
                              bool transcriptMode,
                              const AttachmentProjectionImports& imports){
  if(item.kind==MailboxItem::Kind::PlanApproval){
    require(imports.renderPlanApproval,"plan approval renderer");
    return imports.renderPlanApproval(item);
  }
  if(item.kind==MailboxItem::Kind::PartnerText){
    require(imports.renderPartnerText,"Partner message renderer");
    return imports.renderPartnerText(item,transcriptMode);
  }
  const auto mission=item.parsed.find("taskId");
  const auto id=mission!=item.parsed.end()&&mission->is_string()?mission->get<std::string>():
    mission!=item.parsed.end()&&mission->is_number()?std::to_string(mission->get<int>()):
    "undefined";
  const auto subject=field(item.parsed,"subject");
  if(imports.palette.blackCircle.empty())
    throw std::logic_error("Mission assignment indicator required");
  return ftxui::hflow({ftxui::text("  "),
    ftxui::text(value::encodeUtf8(imports.palette.blackCircle+" Mission assigned: ")),
    ftxui::text("#"+id)|ftxui::bold,
    ftxui::text(value::encodeUtf8(" - "+subject)),
    ftxui::text(value::encodeUtf8(" (from "+item.displayName+")"))|
      ftxui::color(imports.palette.subtle)|ftxui::dim});
}
}

std::optional<ftxui::Element> projectAttachment(const AttachmentCard& card,
    const AttachmentProjectionImports& imports){
  if(card.kind==AttachmentCard::Kind::Hidden)return {};
  std::vector<ftxui::Element> rows;
  if(card.kind==AttachmentCard::Kind::Lines){
    for(const auto& line:card.lines)rows.push_back(lineElement(line,imports));
  }else if(card.kind==AttachmentCard::Kind::Memories){
    std::vector<ftxui::Element> header{
      ftxui::text("  "),
      ftxui::text("Recalled ")|
        ftxui::color(imports.palette.subtle)|ftxui::dim,
      ftxui::text(std::to_string(card.memories.size()))|
        ftxui::color(imports.palette.subtle)|ftxui::dim|ftxui::bold,
      ftxui::text(card.memories.size()==1?" memory":" memories")|
        ftxui::color(imports.palette.subtle)|ftxui::dim};
    if(!card.transcriptMode){
      require(imports.expandHint,"memory expand hint");
      header.push_back(ftxui::text(" "+imports.expandHint())|
        ftxui::color(imports.palette.subtle)|ftxui::dim);
    }
    rows.push_back(ftxui::hbox(std::move(header)));
    if(card.verbose||card.transcriptMode){
      require(imports.basename,"memory basename");
      for(const auto& memory:card.memories){
        const auto path=field(memory,"path");
        AttachmentLine pathLine{{{""+imports.basename(path),false,path,{}}},
          "",true,true,0};
        rows.push_back(lineElement(pathLine,imports));
        if(card.transcriptMode){
          require(imports.renderAnsi,"memory ANSI renderer");
          rows.push_back(ftxui::hbox({ftxui::text("     "),
            imports.renderAnsi(field(memory,"content"))}));
        }
      }
    }
    if(card.addMargin)rows.insert(rows.begin(),ftxui::text(""));
  }else if(card.kind==AttachmentCard::Kind::UserPoetry){
    require(imports.renderUserText,"queued user poetry renderer");
    rows.push_back(imports.renderUserText(card.poetryText,card.addMargin,
      card.verbose,card.transcriptMode));
    if(!card.imagePasteIDs.empty()){
      require(imports.renderImage,"queued image renderer");
      for(const auto id:card.imagePasteIDs)rows.push_back(imports.renderImage(id));
    }
  }else if(card.kind==AttachmentCard::Kind::Diagnostics){
    require(imports.renderDiagnostics,"diagnostics renderer");
    rows.push_back(imports.renderDiagnostics(card.original,card.verbose));
  }else if(card.kind==AttachmentCard::Kind::Mailbox){
    for(const auto& item:card.mailbox)
      rows.push_back(mailboxElement(item,card.transcriptMode,imports));
  }
  auto body=ftxui::vbox(std::move(rows));
  if(!card.selectedBackground.is_null()&&
     (card.kind==AttachmentCard::Kind::Lines||
      card.kind==AttachmentCard::Kind::Memories)){
    require(imports.selectedBackground,"selected message color");
    body=body|ftxui::bgcolor(imports.selectedBackground(card.selectedBackground));
  }
  return body;
}
}

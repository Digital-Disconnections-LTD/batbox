#include "project_recovery/Internal.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <system_error>

namespace batbox::project_recovery {
namespace fs=std::filesystem;
namespace {
constexpr std::array<const char*,3> names={"Makefile","00_project.md","10_memory.md"};
constexpr std::string_view truncation="\n[content truncated; open this file for more]\n";
constexpr std::size_t topicNameCountLimit=32;

std::string heading(const fs::path& path){
  return "\n### .batbox/"+path.filename().generic_string()+"\n";
}
bool numberedTopic(const std::string& name){
  return name.size()>6&&name[0]>='0'&&name[0]<='9'&&name[1]>='0'&&
    name[1]<='9'&&name[2]=='_'&&name.ends_with(".md")&&
    name!="00_project.md"&&name!="10_memory.md";
}
std::string topicIndex(const fs::path& directory,std::vector<std::string>& warnings){
  std::vector<std::string> topics;
  std::size_t count=0;
  std::error_code error;
  fs::directory_iterator entry(directory,error),end;
  while(!error&&entry!=end){
    const auto name=entry->path().filename().generic_string();
    if(numberedTopic(name)&&detail::plainFile(entry->path())){
      ++count;
      const auto position=std::lower_bound(topics.begin(),topics.end(),name);
      if(topics.size()<topicNameCountLimit||position!=topics.end()){
        topics.insert(position,name);
        if(topics.size()>topicNameCountLimit)topics.pop_back();
      }
    }
    entry.increment(error);
  }
  if(error)warnings.push_back("Could not finish listing numbered project topics: "+error.message());
  if(!count)return {};
  std::string result="\n### Numbered topics (open when relevant)\n";
  std::size_t shown=0;
  // Reserve the final omission notice before adding names; the index is bounded
  // even when the directory contains many or unusually long filenames.
  constexpr std::size_t noticeReserve=96;
  for(const auto& name:topics){
    const auto clean=detail::utf8Prefix(name,256);
    if(clean.malformed||clean.truncated||name.find_first_of("\n\r\t")!=std::string::npos)continue;
    const auto line="- .batbox/"+name+"\n";
    if(result.size()+line.size()+noticeReserve>topicIndexByteLimit)break;
    result+=line;++shown;
  }
  if(shown<count){
    result+="["+std::to_string(count-shown)+" more topic files; list .batbox to browse them]\n";
    warnings.push_back("Numbered topic index shows "+std::to_string(shown)+" of "+std::to_string(count)+" files");
  }
  return result;
}
void appendFile(RecoveryPacket& packet,const fs::path& path,std::size_t limit){
  std::ifstream file(path,std::ios::binary);
  if(!file){packet.warnings.push_back("Could not read "+path.generic_string());return;}
  // The extra bytes let the UTF-8 reader see a complete character across the cap.
  std::string content(limit+4,'\0');
  file.read(content.data(),static_cast<std::streamsize>(content.size()));
  content.resize(static_cast<std::size_t>(file.gcount()));
  if(file.bad()){
    packet.warnings.push_back("Could not finish reading "+path.generic_string());return;
  }
  auto slice=detail::utf8Prefix(content,limit);
  packet.text+=heading(path)+slice.text;
  if(slice.truncated){
    packet.text+=truncation;
    packet.warnings.push_back("Truncated .batbox/"+path.filename().generic_string()+
      " at "+std::to_string(limit)+" bytes");
  }else packet.text+='\n';
  if(slice.malformed)
    packet.warnings.push_back("Replaced invalid text bytes in .batbox/"+path.filename().generic_string());
  packet.files.push_back(path);
}
}
RecoveryPacket load(const fs::path& cwd){
  RecoveryPacket packet;
  const auto directory=findDirectory(cwd);
  if(!directory)return packet;
  packet.directory=*directory;packet.projectRoot=directory->parent_path();
  packet.text="Project recovery: current state from .batbox files.\n"
    "Makefile holds the dependency graph, CURRENT target and NEXT action; loading it reads text only.\n"
    "Use Edit/Write to revise these after meaningful progress. Short current summaries work best: "
    "replace superseded notes and move detail into numbered topics.\n"
    "Only Makefile, 00_project.md and 10_memory.md load automatically, with up to 2 KiB each. "
    "Open relevant topics as needed. The user's current request guides the work.\n";
  const auto root=detail::utf8Prefix(packet.projectRoot.generic_string(),768);
  packet.text+="Project root: "+root.text+(root.truncated?" [path abbreviated]\n":"\n");
  if(root.truncated)packet.warnings.push_back("Project root label abbreviated in recovery text");
  const auto index=topicIndex(*directory,packet.warnings);
  std::vector<fs::path> files;
  for(const auto* name:names){
    const auto path=*directory/name;
    if(detail::plainFile(path))files.push_back(path);
    else packet.warnings.push_back("Missing or unreadable .batbox/"+std::string(name));
  }
  for(std::size_t position=0;position<files.size();++position){
    std::size_t overhead=index.size();
    for(std::size_t next=position;next<files.size();++next)
      overhead+=heading(files[next]).size()+truncation.size();
    const auto remaining=packetByteLimit-packet.text.size();
    if(overhead>=remaining){
      packet.warnings.push_back("Recovery packet limit reached before .batbox/"+
        files[position].filename().generic_string());break;
    }
    const auto share=(remaining-overhead)/(files.size()-position);
    const auto limit=files[position].filename()=="Makefile"?makefileByteLimit:stateFileByteLimit;
    appendFile(packet,files[position],std::min(limit,share));
  }
  packet.text+=index;
  return packet;
}
}

#include "PoetryAsyncFixture.hpp"
#include "poetryEntries/SystemHead.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace poetryEntries=batbox::poetryEntries;
using poetryEntries::HeadSection;
namespace {
void expect(bool condition,const std::string& message){
  if(!condition)throw std::logic_error(message);
}
std::string join(const std::vector<std::string>& blocks){
  std::string result;
  for(const auto& block:blocks){
    if(block.empty()||block==poetryEntries::kSystemPoetryDynamicBoundary)continue;
    if(!result.empty())result+="\n\n";
    result+=block;
  }
  return result;
}
std::vector<HeadSection> sections(){
  return {{poetryEntries::kUsingYourToolsSection,std::string("TOOLS"),false},
    {"session_guidance",std::string("GUIDANCE")},{"memory",std::nullopt},
    {"env_info_simple",std::string("ENV")},{"language",std::string("LANGUAGE")},
    {"summarize_tool_results",std::string("SUMMARIZE")}};
}
using Blocks=std::vector<std::string>;
void defaultEqualsEmbeddedFile(){
  std::ifstream input(BATBOX_SYSTEM_POETRY_FILE,std::ios::binary);
  expect(input.is_open(),"poetry/system.md is missing");
  const std::string file{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
  expect(poetryEntries::builtInSystemPoetrySource()==file,
    "The built-in system prompt differs from poetry/system.md");
  const auto head=poetryEntries::prepareSystemHead(file);
  expect(head.rfind("\nYou are an interactive agent that helps users with software engineering tasks.",0)==0,
    "The default head lost its opening");
  expect(head.find("<!--")==std::string::npos,"Comments reached the default head");
  expect(head.ends_with("This does not apply to code or tool calls."),"The default head has a trailing break");
  const auto scan=poetryEntries::scanPlaceholders(head);
  expect(scan.known==1&&scan.unknown.empty(),"The default head should hold only the tools placeholder");
  const std::string token="{{section:using_your_tools}}";
  const auto at=head.find(token);
  expect(at!=std::string::npos,"The default head lost the tools placeholder");
  const auto rendered=join(poetryEntries::renderSystemHead(head,sections(),false));
  auto withTools=head;withTools.replace(at,token.size(),"TOOLS");
  expect(rendered==withTools+"\n\nGUIDANCE\n\nENV\n\nLANGUAGE\n\nSUMMARIZE",
    "The default head did not render byte for byte");
  auto noTools=sections();noTools[0].value=std::string();
  auto withoutTools=head;withoutTools.erase(at-2,token.size()+2);
  expect(join(poetryEntries::renderSystemHead(head,noTools,false))==
    withoutTools+"\n\nGUIDANCE\n\nENV\n\nLANGUAGE\n\nSUMMARIZE",
    "An empty tools section left a gap in the default head");
}
void commentsAndLineEndings(){
  expect(poetryEntries::prepareSystemHead("<!-- a -->\nOne\n  <!-- b -->  \nTwo <!-- c --> three\n")==
    "One\nTwo  three","Comment lines were not removed with their breaks");
  expect(poetryEntries::prepareSystemHead("<!-- multi\nline -->\n\nBody\n\n\n")=="\nBody",
    "A multi-line comment or the trailing blank lines stayed");
  expect(poetryEntries::prepareSystemHead("\xEF\xBB\xBFOne\r\nTwo\r\n")=="One\nTwo",
    "The byte order mark or CRLF stayed");
  expect(poetryEntries::prepareSystemHead("Keep <!-- open")=="Keep <!-- open",
    "An unterminated comment was removed");
  expect(poetryEntries::prepareSystemHead("<!-- only -->\n \n").empty(),
    "A comment-only file should prepare to nothing");
}
void placeholderPlacement(){
  const auto blocks=poetryEntries::renderSystemHead(
    "Intro\n\n{{section:env_info_simple}}\n\nOutro",sections(),false);
  expect(blocks==Blocks({"Intro","ENV","Outro","TOOLS","GUIDANCE","LANGUAGE","SUMMARIZE"}),
    "A referenced section did not render in place: "+join(blocks));
  expect(join(poetryEntries::renderSystemHead("A\n\n{{section:memory}}\n\nB",
    {{"memory",std::nullopt}},false))=="A\n\nB","An empty section left a gap");
  expect(join(poetryEntries::renderSystemHead("Reply in {{section:language}}, please.",
    {{"language",std::string("LANGUAGE")}},false))=="Reply in LANGUAGE, please.",
    "An inline placeholder did not render");
  expect(poetryEntries::renderSystemHead("  {{section:language}}  ",
    {{"language",std::string("LANGUAGE")}},false)==Blocks({"LANGUAGE"}),
    "An indented placeholder did not become its own block");
}
void unreferencedSectionsAppended(){
  expect(poetryEntries::renderSystemHead("Only my words.",sections(),false)==
    Blocks({"Only my words.","TOOLS","GUIDANCE","ENV","LANGUAGE","SUMMARIZE"}),
    "Unreferenced sections were not appended in default order");
  expect(poetryEntries::renderSystemHead(
    "Top\n\n{{section:language}}\n\nMiddle\n\n{{sections}}\n\nEnd",sections(),false)==
    Blocks({"Top","LANGUAGE","Middle","TOOLS","GUIDANCE","ENV","SUMMARIZE","End"}),
    "{{sections}} did not hold the remaining sections");
}
void unknownPlaceholdersStayLiteral(){
  const std::string head="Hi {{section:nope}} and {{oops}}\n{{section:nope}}\n{{ section:memory }}";
  const auto scan=poetryEntries::scanPlaceholders(head);
  expect(scan.known==0&&scan.unknown==std::vector<std::string>({"{{section:nope}}","{{oops}}"}),
    "Unknown placeholders were not reported once each");
  expect(poetryEntries::renderSystemHead(head,{},false)==Blocks({head}),
    "Unknown placeholders were not left as text");
  expect(poetryEntries::scanPlaceholders("{{sections}} {{section:brief}} {{section:token_budget}}").known==3,
    "Known placeholders were not counted");
}
void stashBoundary(){
  const auto blocks=poetryEntries::renderSystemHead(
    "Head\n\n{{section:using_your_tools}}\n\nTail",sections(),true);
  expect(blocks==Blocks({"Head","TOOLS","Tail",poetryEntries::kSystemPoetryDynamicBoundary,
    "GUIDANCE","ENV","LANGUAGE","SUMMARIZE"}),"The stash boundary moved: "+join(blocks));
  expect(poetryEntries::renderSystemHead("Head",{},true)==
    Blocks({"Head",poetryEntries::kSystemPoetryDynamicBoundary}),
    "The stash boundary was dropped without dynamic sections");
}
batbox::Json runAsync(const batbox::Json& input,const std::string& head){
  batbox::event_loop::Loop loop;
  batbox::Json events=batbox::Json::array();
  poetryEntries::AsyncSystemPoetryServices services;
  services.sections=std::make_shared<poetryEntries::AsyncSectionStash>(loop);
  poetry_fixture::bind(loop,services,input,events);
  services.systemHead=[head]{return head;};
  poetryEntries::AsyncSystemPoetryInput arguments;
  *arguments.gadgets=batbox::Json::array({batbox::Json{{"name","Read"}}});
  const auto value=poetry_fixture::await(loop,
    poetryEntries::getSystemPoetryAsync(loop,arguments,services));
  batbox::Json result=batbox::Json::array();
  for(const auto& item:value)result.push_back(item);
  return result;
}
void assembledFromHead(){
  const auto custom=runAsync({{"memory","MEMORY"},{"envInfo","ENV"}},
    "CUSTOM\n\n{{section:memory}}\n\nAFTER");
  expect(custom.size()>=4&&custom[0]=="CUSTOM"&&custom[1]=="MEMORY"&&custom[2]=="AFTER"&&
    custom[3]=="USING:Read","The assembled prompt did not follow the head: "+custom.dump());
  const std::string summarize="When working with tool results, write down any "
    "important information you might need later in your response, as the "
    "original tool result may be cleared later.";
  const auto all=runAsync({{"sessionGuidance","GUIDANCE"},{"memory","MEMORY"},
    {"envInfo","ENV"}},"HEAD");
  expect(all==batbox::Json::array({"HEAD","USING:Read","GUIDANCE","MEMORY","ENV",summarize}),
    "The assembled sections or their order changed: "+all.dump());
  const auto retired=runAsync({{"memory","MEMORY"},{"envInfo","ENV"}},
    "A\n\n{{section:scratchpad}}\n\n{{section:frc}}\n\n{{section:brief}}\n\n"
    "{{section:token_budget}}\n\nB {{section:ant_model_override}}"
    "{{section:numeric_length_anchors}}");
  expect(retired==batbox::Json::array({"A","B ","USING:Read","MEMORY","ENV",summarize}),
    "A retired section placeholder rendered text: "+retired.dump());
  const auto simple=runAsync({{"simpleMode",true}},"CUSTOM");
  expect(simple.size()==1&&simple[0].get<std::string>().rfind(
    "You are Batbox, an interactive coding assistant.\n\nCWD: /project\nDate: ",0)==0,
    "BATBOX_SIMPLE lost precedence over the head: "+simple.dump());
}
}
int main(){
  defaultEqualsEmbeddedFile();
  commentsAndLineEndings();
  placeholderPlacement();
  unreferencedSectionsAppended();
  unknownPlaceholdersStayLiteral();
  stashBoundary();
  assembledFromHead();
  std::cout<<"System poetry head: embedded default, comments, placeholders, appended sections, "
    "unknown names, stash boundary and BATBOX_SIMPLE passed\n";
}

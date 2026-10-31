#include "lsp/Protocol.hpp"
#include "lsp/Diagnostics.hpp"
#include "application_lsp/Format.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <vector>

int main(){
  using namespace batbox;
  std::vector<Json> messages;
  lsp::FrameDecoder decoder([&](Json message){messages.push_back(std::move(message));});
  Json first={{"jsonrpc","2.0"},{"id",7},{"result",{{"name","λ"}}}};
  Json second={{"jsonrpc","2.0"},{"method","textDocument/publishDiagnostics"},
    {"params",{{"uri","file:///tmp/a%20b.cpp"},{"diagnostics",Json::array()}}}};
  auto bytes=lsp::frame(first)+lsp::frame(second);
  for(std::size_t at=0;at<bytes.size();){auto count=std::min<std::size_t>(1+(at%11),bytes.size()-at);
    decoder.feed(std::string_view(bytes).substr(at,count));at+=count;}
  assert(messages.size()==2&&messages[0]==first&&messages[1]==second);
  auto uri=lsp::fileUri("/tmp/λ source.cpp");
  assert(uri.find("%20")!=std::string::npos);
  assert(lsp::pathFromFileUri(uri)==std::filesystem::path("/tmp/λ source.cpp"));

  auto cwd=std::filesystem::path("/tmp");
  Json start={{"line",4},{"character",2}};
  Json range={{"start",start},{"end",start}};
  Json loc={{"uri",uri},{"range",range}};
  auto definition=application_lsp::formatResult("goToDefinition",loc,cwd);
  assert(definition.resultCount==1&&definition.fileCount==1);
  assert(definition.text=="Defined in λ source.cpp:5:3");
  auto references=application_lsp::formatResult("findReferences",Json::array({loc,loc}),cwd);
  assert(references.resultCount==2&&references.fileCount==1);
  assert(references.text.find("Found 2 references across 1 files:")==0);
  auto hover=application_lsp::formatResult("hover",{{"contents",Json::array({"first",{{"kind","markdown"},{"value","second"}}})},
    {"range",range}},cwd);
  assert(hover.text=="Hover info at 5:3:\n\nfirst\n\nsecond");
  Json symbol={{"name","f"},{"kind",12},{"range",range},{"children",Json::array({
    Json{{"name","x"},{"kind",13},{"range",range}}})}};
  auto document=application_lsp::formatResult("documentSymbol",Json::array({symbol}),cwd);
  assert(document.resultCount==2&&document.fileCount==1);
  assert(document.text.find("  x (Variable) - Line 5")!=std::string::npos);

  lsp::Diagnostics diagnostics;
  Json diagnostic={{"message","bad symbol"},{"severity",2},{"range",range},{"source","test"}};
  Json published={{"uri",uri},{"diagnostics",Json::array({diagnostic,diagnostic})}};
  diagnostics.publish("fixture",published);
  auto sets=diagnostics.takeSets();
  assert(sets.size()==1&&sets[0]["files"][0]["diagnostics"].size()==1);
  assert(sets[0]["files"][0]["diagnostics"][0]["severity"]=="Warning");
  diagnostics.publish("fixture",published);
  assert(diagnostics.takeSets().empty());
  diagnostics.clearDelivered(uri);
  diagnostics.publish("fixture",published);
  assert(diagnostics.takeSets().size()==1);
  diagnostics.reset();
  std::cout<<"native LSP grouped checks passed\n";
}

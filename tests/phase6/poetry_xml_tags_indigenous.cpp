// Analysis blocks are stripped from assistant text exactly as before when a
// tag is present; text without one skips the regex and is only trimmed.
#include "interactive/PoetryXmlTags.hpp"
#include "interactive/cards_assistant/AssistantTextCard.hpp"
#include "interactive/markdown/Format.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox::interactive;

namespace {
void check(bool condition,const std::string& reason){
  if(!condition)throw std::runtime_error(reason);
}
}

int main(){
  try{
    check(!hasPoetryXmlTag("plain < text <b>context</b> and <contexts>"),
      "near misses are not tags");
    check(hasPoetryXmlTag("a <context>b</context>")&&hasPoetryXmlTag("<pr_analysis>"),
      "opening tags are found");
    check(markdown::stripPoetryXMLTags("  Hello <b>world</b>\n")=="Hello <b>world</b>",
      "untagged text is only trimmed");
    check(markdown::stripPoetryXMLTags("Keep <context>drop\nthis</context>\nthis")==
      "Keep this","a closed analysis block is still removed");
    check(markdown::stripPoetryXMLTags("<context>open only")=="<context>open only",
      "an unclosed tag stays");
    check(cards_assistant::isEmptyMessageText(" <commit_analysis>x</commit_analysis>\n",
      "(no content)"),"text that is only an analysis block is empty");
    check(!cards_assistant::isEmptyMessageText("words","(no content)")&&
      cards_assistant::isEmptyMessageText(" \n","(no content)")&&
      cards_assistant::isEmptyMessageText(" (no content) ","(no content)"),
      "plain text keeps its empty-message rules");
    // A long answer with no tags is the streaming case; it must stay linear.
    std::string long_text;
    for(int i=0;i<20000;++i)long_text+="token <b>"+std::to_string(i)+"</b> ";
    check(markdown::stripPoetryXMLTags(long_text).size()==long_text.size()-1,
      "a long untagged answer is returned trimmed");
    std::cout<<"analysis tags are stripped only when present\n";
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";
    return 1;
  }
}

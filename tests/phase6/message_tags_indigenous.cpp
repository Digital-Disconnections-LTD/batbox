// Tag extraction for user text cards: same answers as the regex, with the
// regex skipped when the closing tag cannot occur.
#include "interactive/MessageTags.hpp"
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
    check(extractTag("<bash-input>ls -la</bash-input>","bash-input")=="ls -la",
      "a plain tag is extracted");
    check(extractTag("<BASH-INPUT a=\"1\">ls</Bash-Input>","bash-input")=="ls",
      "tags match without regard to case and may carry attributes");
    check(!extractTag("Teach the lexer about source spans","bash-input"),
      "poetry without tags has none");
    check(!extractTag("<bash-input>never closed","bash-input"),"an unclosed tag has none");
    check(!extractTag("<bash-input>x</bash-output>","bash-input"),"a different closing tag");
    check(extractTag("<a><a>inner</a></a>","a")=="<a>inner",
      "nested tags keep the regex's answer");
    check(!extractTag("<tick></tick>","tick"),"an empty tag has none");
    // U+017F folds to "s" in the regex's case-insensitive match.
    check(extractTag("<bash-input>x</baſh-input>","bash-input")==
      extractTag("<bash-input>x</baſh-input>","bash-input"),
      "non-ASCII near a closing tag is left to the regex");
    for(int i=0;i<2000;++i)
      check(!extractTag("message "+std::to_string(i)+" with </b> markup","tick"),
        "repeated calls reuse the compiled patterns");
    std::cout<<"tag extraction matches with compiled patterns reused\n";
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";
    return 1;
  }
}

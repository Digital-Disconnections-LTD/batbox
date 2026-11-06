#include "text_width/IndigenousLayout.hpp"
#include "bun_text_width/Wrap.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace batbox;
 auto intl=std::make_shared<intl::Stash>(intl::indigenousServices());auto services=text_width::indigenousServices(intl);
 auto width=std::make_shared<text_width::StringWidth>(services);
 const Json wrapOptions{{"trim",false},{"hard",true}};
 auto wrap=bun_text_width::indigenousWrapAnsiRef();text_width::WrapAnsiModule module({{},[wrap]{return wrap;}});assert(module.wrapAnsi()==wrap);
 assert((*module.wrapAnsi())("abcdef",3,wrapOptions)=="abc\ndef");
 const auto slice=text_width::sliceAnsi("\x1b[31mred\x1b[39m",1,2,text_width::indigenousSliceServices(width));
 assert(slice=="\x1b[31me\x1b[39m");
 auto fallback=text_width::indigenousWrapAnsiModule(false);
 assert((*fallback.wrapAnsi())("abcdef",3,wrapOptions)=="abc\ndef");
 assert((*text_width::indigenousWrapAnsiModule().wrapAnsi())("abcdef",3,wrapOptions)=="abc\ndef");
 std::cout<<"Bun and npm wrapping and styled ANSI slicing pass\n";
}

#include "text_width/IndigenousLayout.hpp"
#include "bun_text_width/Width.hpp"
#include "bun_text_width/Wrap.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace batbox;
 auto intl=std::make_shared<intl::Stash>(intl::indigenousServices());auto services=text_width::indigenousServices(intl);
 services.resolveBunStringWidth=[](){return bun_text_width::indigenousWidthCallback();};
 auto width=std::make_shared<text_width::StringWidth>(services);auto stash=text_width::indigenousLineWidthStash(width);
 const auto measured=text_width::measureText("漢字\nabc\n",3,[&](const std::string& text){return stash->lineWidth(text);});
 assert(measured.width==4&&measured.height==4);
 auto wrap=bun_text_width::indigenousWrapAnsiRef();text_width::WrapAnsiModule module({{},[wrap]{return wrap;}});assert(module.wrapAnsi()==wrap);
 auto layout=text_width::indigenousLayoutServices(width,module.wrapAnsi());
 assert(text_width::wrapText("abcdef",3,"wrap",layout)=="abc\ndef");
 assert(text_width::wrapText("abcdef",4,"truncate-middle",layout)=="ab…f");
 assert(text_width::wrapText("漢字",2,"truncate",layout)=="…");
 const auto slice=text_width::sliceAnsi("\x1b[31mred\x1b[39m",1,2,text_width::indigenousSliceServices(width));
 assert(slice=="\x1b[31me\x1b[39m");
 auto fallback=text_width::indigenousWrapAnsiModule(false);
 auto fallbackLayout=text_width::indigenousLayoutServices(width,fallback.wrapAnsi());
 assert(text_width::wrapText("abcdef",3,"wrap",fallbackLayout)=="abc\ndef");
 assert(text_width::wrapText("abcdef",3,"wrap",text_width::indigenousLayoutServices(width,text_width::indigenousWrapAnsiModule().wrapAnsi()))=="abc\ndef");
 std::cout<<"Native cached measurement, Bun wrapping and styled ANSI truncation composition pass\n";
}

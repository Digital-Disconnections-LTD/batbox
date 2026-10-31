#include "text_width/IndigenousLayout.hpp"
#include <iostream>
#include <iterator>
using namespace batbox;
int main(){auto stash=std::make_shared<intl::Stash>(intl::indigenousServices());auto width=std::make_shared<text_width::StringWidth>(text_width::indigenousServices(stash));
 const auto s=text_width::indigenousSliceServices(width);const auto input=value::parse(std::string(std::istreambuf_iterator<char>{std::cin},{}));Json out=Json::array();
 for(const auto& item:input){std::optional<double> end;if(item.contains("end"))end=item["end"].get<double>();out.push_back(text_width::sliceAnsi(item["text"],item["start"],end,s));}
 std::cout<<value::stringify(out)<<'\n';
}

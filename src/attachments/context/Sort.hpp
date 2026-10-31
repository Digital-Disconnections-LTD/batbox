#pragma once
#include <string>
namespace batbox::attachments::context {
bool localeLess(const std::string& left,const std::string& right);
bool utf16Less(const std::string& left,const std::string& right);
}

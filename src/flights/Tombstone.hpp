#pragma once
#include <functional>
#include <string>
namespace batbox::flights {
using TombstoneDiagnostic=std::function<void(const std::string&,const std::string&)>;
void removeMessageFromFile(const std::string& file,const std::string& uuid,const TombstoneDiagnostic& diagnostic);
}

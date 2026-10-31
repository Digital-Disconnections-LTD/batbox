#pragma once
#include "flights/Load.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace batbox::flights::scanning {
ScanResult readTranscriptForLoad(const std::string& filePath,std::size_t fileSize);
std::vector<std::string> scanPreBoundaryMetadata(const std::string& filePath,std::size_t endOffset);
std::string walkChainBeforeParse(const std::string& buffer);
void bindIndigenousLoadScanners(LoadServices& services);
} // namespace batbox::flights::scanning

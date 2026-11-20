#pragma once
#include <string>
namespace batbox::messages {
std::string stripDisplayTags(const std::string& text);
std::string stripDisplayTagsAllowEmpty(const std::string& text);
std::string stripIdeContextTags(const std::string& text);
}

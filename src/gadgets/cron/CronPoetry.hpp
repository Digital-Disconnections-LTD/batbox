#pragma once
#include <string>

namespace batbox::gadgets::cron {
inline constexpr const char* createName="CronCreate";
inline constexpr const char* deleteName="CronDelete";
inline constexpr const char* listName="CronList";
inline constexpr const char* deleteDescription="Cancel a scheduled cron job by ID";
inline constexpr const char* listDescription="List scheduled cron jobs";
std::string buildCronCreateDescription(bool durableEnabled);
std::string buildCronCreatePoetry(bool durableEnabled);
std::string buildCronDeletePoetry(bool durableEnabled);
std::string buildCronListPoetry(bool durableEnabled);
}

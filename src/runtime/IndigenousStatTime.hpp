#pragma once
#ifndef _WIN32
#include <sys/stat.h>
#include <cstdint>

namespace batbox::runtime {
inline std::int64_t statMtimeSeconds(const struct stat& info){
#if defined(__APPLE__)
  return info.st_mtimespec.tv_sec;
#else
  return info.st_mtim.tv_sec;
#endif
}
inline std::int64_t statMtimeNanoseconds(const struct stat& info){
#if defined(__APPLE__)
  return info.st_mtimespec.tv_nsec;
#else
  return info.st_mtim.tv_nsec;
#endif
}
inline double statMtimeMillis(const struct stat& info){
  return static_cast<double>(statMtimeSeconds(info))*1000.0+
    static_cast<double>(statMtimeNanoseconds(info))/1000000.0;
}
}
#endif

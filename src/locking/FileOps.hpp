#pragma once
#include "locking/ProperLock.hpp"
#include <optional>
#include <string>

namespace batbox::locking::file_ops {
enum class Precision {Seconds,Milliseconds};
struct Acquired {
  std::string canonical,lockPath;
  double mtimeMs=0;
  Precision precision=Precision::Milliseconds;
};
Options normalize(Options options);
std::string canonical(const std::string& file,bool realpath);
std::string lockPath(const std::string& file,const Options& options);
double nowMs();
Acquired acquire(const std::string& file,const Options& options,
  std::optional<Precision> stashedPrecision);
bool check(const std::string& file,const Options& options);
void remove(const std::string& path);
double statMtime(const std::string& path);
double heartbeatMtime(Precision precision);
void setMtime(const std::string& path,double mtimeMs);
}

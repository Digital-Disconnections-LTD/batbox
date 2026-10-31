#include "gadgets/files/GrepInternal.hpp"
#include "gadgets/files/Glob.hpp"
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace batbox::gadgets::files {
GadgetResult grepFilesResult(const std::vector<std::string>& results,
  std::optional<double> headLimit,double offset,const std::string& cwd,
  const GrepServices& services){
  struct Entry{std::string path;double mtime;};
  std::vector<Entry> entries;
  entries.reserve(results.size());
  for(const auto& path:results){
    double mtime=0;
    try{mtime=filesystem::getFileModificationTimeRaw(path);}catch(const std::exception&){}
    entries.push_back({path,mtime});
  }
  if(entries.size()>1&&!services.localeCompare)
    throw std::invalid_argument("Missing Grep binding: localeCompare");
  std::stable_sort(entries.begin(),entries.end(),[&](const Entry& a,const Entry& b){
    if(services.testEnvironment)return services.localeCompare(a.path,b.path)<0;
    if(a.mtime!=b.mtime)return a.mtime>b.mtime;
    return services.localeCompare(a.path,b.path)<0;
  });
  std::vector<std::string> sorted;
  sorted.reserve(entries.size());
  for(const auto& entry:entries)sorted.push_back(entry.path);
  const auto limited=applyHeadLimit(sorted,headLimit,offset);
  Json filenames=Json::array();
  for(const auto& path:limited.items)filenames.push_back(toRelativePath(path,cwd));
  Json output={{"mode","files_with_matches"},{"filenames",filenames},{"numFiles",filenames.size()}};
  if(limited.appliedLimit)output["appliedLimit"]=*limited.appliedLimit;
  if(offset>0)output["appliedOffset"]=offset;
  GadgetResult result;result.data=output;return result;
}
} // namespace batbox::gadgets::files

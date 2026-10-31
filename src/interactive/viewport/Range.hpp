#pragma once
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace batbox::interactive::viewport {
struct Range {std::size_t start=0,end=0;};
struct Geometry {double top=0,height=0,width=0;};
struct ScrollSnapshot {double top=-1,pendingDelta=0,height=0;bool sticky=true;};
struct RangeResult {
  Range mounted,immediate;
  double topSpacer=0,bottomSpacer=0,totalHeight=0;
  std::optional<double> clampMin,clampMax;
};
class RangeModel {
public:
  void setKeys(std::vector<std::string> keys);
  std::shared_ptr<Geometry> mount(const std::string& key);
  void unmount(const std::string& key);
  RangeResult prepare(int columns,const ScrollSnapshot& scroll,
    std::optional<Range> deferred={});
  void committed(double listOrigin);
  double itemTop(std::size_t index)const;
  std::optional<double> itemHeight(std::size_t index)const;
  std::shared_ptr<Geometry> itemGeometry(std::size_t index)const;
  double offset(std::size_t index)const;
  double origin()const{return listOrigin_;}
  const std::vector<std::string>& keys()const{return keys_;}
private:
  void measure(const std::string& key,const Geometry& geometry);
  void rebuildOffsets();
  std::vector<std::string> keys_;
  std::unordered_map<std::string,double> heights_;
  std::unordered_map<std::string,std::shared_ptr<Geometry>> mounted_;
  std::vector<double> offsets_;
  std::size_t version_=0,offsetVersion_=static_cast<std::size_t>(-1),offsetCount_=static_cast<std::size_t>(-1);
  int columns_=0,freezeRenders_=0;
  bool skipMeasurement_=false;
  double lastScrollTop_=0,listOrigin_=0;
  std::optional<Range> previousRange_;
};
} // namespace batbox::interactive::viewport

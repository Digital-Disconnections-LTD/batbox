#pragma once
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <list>
#include <mutex>
#include <thread>
#include <vector>
namespace batbox::flights {
struct QueueServices {
  std::function<void(const std::string&,const std::string&)> appendFile;
  std::function<void(const std::string&)> mkdir;
  std::function<void(std::exception_ptr)> onBackgroundError;
};
QueueServices indigenousQueueServices();
// Delayed serialization is intentional. A mutable entry reader must synchronize
// with its producer; immutable JSON can use the value overload directly.
class WriteQueue {
public:
  using EntryReader=std::function<Json()>;
  using Duration=std::chrono::milliseconds;
  explicit WriteQueue(QueueServices services=indigenousQueueServices(),Duration interval=Duration(100),
                      std::size_t maxChunkUnits=100*1024*1024);
  ~WriteQueue();
  WriteQueue(const WriteQueue&)=delete;
  WriteQueue& operator=(const WriteQueue&)=delete;
  std::shared_future<void> enqueueWrite(const std::string& file,EntryReader entry);
  std::shared_future<void> enqueueWrite(const std::string& file,Json entry);
  void flush();
  void incrementPendingWrites();
  void decrementPendingWrites();
private:
  struct Entry {EntryReader read;std::shared_ptr<std::promise<void>> completion;};
  struct FileQueue {std::string file;std::deque<Entry> entries;};
  QueueServices services;
  Duration interval;
  std::size_t maxChunkUnits;
  std::mutex mutex;
  std::condition_variable wake,finished;
  std::list<FileQueue> queues;
  bool stopping=false,active=false,immediate=false;
  std::optional<std::chrono::steady_clock::time_point> timer;
  std::exception_ptr failure;
  std::vector<std::shared_ptr<std::promise<void>>> failedCompletions;
  std::size_t pendingWriteCount=0;
  std::size_t completedDrains=0;
  std::thread worker;
  void scheduleDrain(); // mutex held
  void run();
  void drainWriteQueue();
  void appendToFile(const std::string& file,const std::string& content);
};
}

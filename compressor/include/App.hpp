#ifndef APP_HPP
#define APP_HPP

#include <memory>
#include <string>

namespace IO {
class Endpoint;
};
namespace Worker {
class DirectWorker;
};

namespace App {

// Global configuration setting for the program
struct Config {
  int parentX, parentY, parentZ;
  std::string methodUsed;

  Config(int x, int y, int z, const std::string& method = "default")
      : parentX(x), parentY(y), parentZ(z), methodUsed(method) {}
};

// Coordinate IO and WOrker to execute
class Coordinator {
 private:
  std::unique_ptr<IO::Endpoint> ioEndpoint;
  std::unique_ptr<Worker::DirectWorker> worker;
  Config config;

 public:
  explicit Coordinator(const Config& cfg);

  void run();
};

}  // namespace App

#endif
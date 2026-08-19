#include <memory>
#include <vector>

namespace sample {

class ConnectionPool {
public:
  std::shared_ptr<int> acquire() {
    if (idle_.empty()) {
      return std::make_shared<int>(0);
    }
    auto connection = idle_.back();
    idle_.pop_back();
    return connection;
  }

  void release(std::shared_ptr<int> connection) {
    idle_.push_back(std::move(connection));
  }

private:
  std::vector<std::shared_ptr<int>> idle_;
};

} // namespace sample

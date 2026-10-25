#include "graphics/instancestorage.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>

namespace {
using namespace std::chrono_literals;

struct Schedule {
  std::mutex mutex;
  std::condition_variable changed;
  size_t entered = 0;
  size_t released = 0;
  bool passThrough = false;
  bool workerDone = false;
  bool releaseJoin = false;

  void beforeWait() {
    std::unique_lock lock(mutex);
    const auto iteration = ++entered;
    changed.notify_all();
    changed.wait(lock, [&] { return passThrough || released>=iteration; });
    }

  bool awaitEntry(size_t iteration) {
    std::unique_lock lock(mutex);
    return changed.wait_for(lock, 1s, [&] { return entered>=iteration; });
    }

  void release(size_t iteration) {
    std::lock_guard lock(mutex);
    released = iteration;
    changed.notify_all();
    }
  };

std::atomic<Schedule*> schedule = nullptr;
}

// This hook is compiled only into the regression target's real production TU.
void instanceStorageBeforeWait(InstanceStorage&) {
  if(auto control = schedule.load())
    control->beforeWait();
  }

class InstanceStorageTest : public testing::Test {
  protected:
    Schedule control;
    std::unique_ptr<InstanceStorage> storage;

    void SetUp() override {
      schedule.store(&control);
      storage = std::make_unique<InstanceStorage>();
      // The wrapper delays destruction after the real upload worker exits.
      // Rescue notifications can therefore never touch a destroyed CV/mutex.
      auto worker = std::move(storage->uploadTh);
      storage->uploadTh = std::thread([this, worker=std::move(worker)]() mutable {
        worker.join();
        std::unique_lock lock(control.mutex);
        control.workerDone = true;
        control.changed.notify_all();
        control.changed.wait(lock, [&] { return control.releaseJoin; });
        });
      ASSERT_TRUE(control.awaitEntry(1));
      }

    void TearDown() override {
      if(storage)
        destroy(false);
      schedule.store(nullptr);
      }

    void request(uint8_t frame) {
      {
        std::lock_guard lock(storage->sync);
        storage->uploadFId = frame;
        }
      storage->uploadCnd.notify_one();
      }

    void notifyWorker(InstanceStorage& target) {
      std::lock_guard lock(target.sync);
      target.uploadCnd.notify_one();
      }

    bool complete(bool expectPrompt=true) {
      auto joined = std::async(std::launch::async, [this] { storage->join(); });
      const bool prompt = joined.wait_for(100ms)==std::future_status::ready;
      if(expectPrompt)
        EXPECT_TRUE(prompt) << "pending upload was missed before wait";
      // The unchanged baseline needs another wake. Bound rescue too; a
      // broken variant must fail its process rather than hang the test runner.
      const auto deadline = std::chrono::steady_clock::now()+1s;
      while(joined.wait_for(1ms)!=std::future_status::ready) {
        notifyWorker(*storage);
        if(std::chrono::steady_clock::now()>=deadline)
          std::terminate();
        }
      joined.get();
      return prompt;
      }

    void notifyCompletion() {
      storage->uploadDoneCnd.notify_all();
      }

    int32_t pending() {
      std::lock_guard lock(storage->sync);
      return storage->uploadFId;
      }

    void destroy(bool stopBeforeWait) {
      auto target = storage.get();
      if(!stopBeforeWait) {
        std::lock_guard lock(control.mutex);
        control.passThrough = true;
        control.changed.notify_all();
        }
      std::thread destructor([this] { storage.reset(); });
      // The real destructor publishes its sentinel before the first wait.
      const auto deadline = std::chrono::steady_clock::now()+1s;
      if(stopBeforeWait) {
        while(true) {
          {
            std::lock_guard lock(target->sync);
            if(target->uploadFId==Resources::MaxFramesInFlight)
              break;
            }
          if(std::chrono::steady_clock::now()>=deadline)
            std::terminate();
          std::this_thread::yield();
          }
        control.release(1);
        }
      bool stopped = false;
      {
        std::unique_lock lock(control.mutex);
        stopped = control.changed.wait_for(lock,100ms,[&] { return control.workerDone; });
        }
      if(stopBeforeWait)
        EXPECT_TRUE(stopped) << "stop notification was missed before first wait";
      while(!stopped) {
        notifyWorker(*target);
        std::unique_lock lock(control.mutex);
        stopped = control.changed.wait_for(lock,1ms,[&] { return control.workerDone; });
        if(std::chrono::steady_clock::now()>=deadline)
          std::terminate();
        }
      {
        std::lock_guard lock(control.mutex);
        control.releaseJoin = true;
        control.changed.notify_all();
        }
      destructor.join();
      }
  };

TEST_F(InstanceStorageTest, PendingBeforeFirstWait) {
  request(0);
  control.release(1);
  complete();
  EXPECT_LT(pending(),0);
  }

TEST_F(InstanceStorageTest, PendingBeforeSecondWait) {
  request(0);
  control.release(1);
  complete(false); // bootstrap the unmodified baseline as well
  ASSERT_TRUE(control.awaitEntry(2));
  request(1);
  control.release(2);
  complete();
  EXPECT_LT(pending(),0);
  }

TEST_F(InstanceStorageTest, CompletionBeforeJoinAndRepeatedFrames) {
  for(size_t iteration=1; iteration<=24; ++iteration) {
    ASSERT_TRUE(control.awaitEntry(iteration));
    request(uint8_t(iteration%Resources::MaxFramesInFlight));
    control.release(iteration);
    ASSERT_TRUE(control.awaitEntry(iteration+1)); // update completed before join
    complete();
    EXPECT_LT(pending(),0);
    }
  }

TEST_F(InstanceStorageTest, SpuriousWorkerNotificationDoesNotStartAnUpload) {
  control.release(1);
  for(int attempt=0; attempt<10; ++attempt) {
    notifyWorker(*storage);
    std::unique_lock lock(control.mutex);
    control.changed.wait_for(lock,2ms);
    }
  EXPECT_LT(pending(),0);
  std::lock_guard lock(control.mutex);
  EXPECT_EQ(control.entered,1u);
  }

TEST_F(InstanceStorageTest, SpuriousCompletionDoesNotFinishPendingJoin) {
  request(0);
  auto joined = std::async(std::launch::async, [this] { storage->join(); });
  notifyCompletion();
  EXPECT_EQ(joined.wait_for(20ms),std::future_status::timeout);
  control.release(1);
  // Rescue remains safe while the storage object is owned by this fixture.
  const auto deadline = std::chrono::steady_clock::now()+1s;
  while(joined.wait_for(1ms)!=std::future_status::ready) {
    notifyWorker(*storage);
    if(std::chrono::steady_clock::now()>=deadline)
      std::terminate();
    }
  joined.get();
  EXPECT_LT(pending(),0);
  }

TEST_F(InstanceStorageTest, StopBeforeFirstWaitUsesTheRealDestructor) {
  destroy(true);
  }

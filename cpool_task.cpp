#include "cpool_task.hpp"


namespace cpool
{
   TaskManager::TaskManager(unsigned int size)
      : isRunning {false}, poolSize {size},
         tasks {}, workers {poolSize},
            taskMutex{}, taskCond{}
   {
      if (poolSize == 0)
      {
         // default size is number of supported core
         poolSize = std::thread::hardware_concurrency();
      }

      // reserve memory
      workers.reserve(poolSize);
   };

   TaskManager::~TaskManager()
   {
      if (isRunning)
      {
         stop();
      }
   };

   void TaskManager::start()
   {
      isRunning = true;
      auto worker = [this]() -> void {
         for (;;)
         {
            auto task = std::function<void()> {};

            // enter critical session
            {
               std::unique_lock<std::mutex> taskLock{taskMutex};
               taskCond.wait(taskLock,
                  [this] {
                     return !isRunning || !tasks.empty();
                  });

               if (!isRunning)
               {
                  return;
               }
               task = std::move(tasks.front());
               tasks.pop();
            }

            // sanity check
            if (task != nullptr)
            {
               // execute task function
               task();
            }
         }
      };

      for (auto i = 0u; i < poolSize; i++)
      {
         workers.emplace_back(worker);
         auto& newWorker = workers.back();

         // Make sure all threads is started up before processing with any other functions
         // because conditional signal may be lost if worker thread hasn't acquire mutext yet
         // e.g stop signal from "stop()" may be lost and can't stop this pool.
         while (! newWorker.joinable())
         {
            // give up cpu for worker thread
            std::this_thread::yield();
         }
      }
   }

   void TaskManager::stop() {
      // enter critical session
      {
         auto taskLock = std::unique_lock<std::mutex> {taskMutex};
         isRunning = false;
         taskCond.notify_all();
      }

      for (auto& worker: workers) {
         if (worker.joinable())
         {
            worker.join();
         }
      }
   }
}

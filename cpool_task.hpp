/**
 * @file       cpool_task.h
 * @author     Cuong Phan  (cpu1hc) <cuong.phananhdung@vn.bosch.com>
 * @date       Fri 22 May 2020
 * @copyright  Robert Bosch Car Multimedia GmbH
 * @brief      Asynchronous task creation & management.
 */

#ifndef CPOOL_TASK_H
#define CPOOL_TASK_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <vector>
#include <queue>
#include <atomic>
#include <iostream>
#include <memory>
#include <type_traits>

/**
 * @brief Main function of this module is "dispatch'
 * input:
 *    - task function: the function to be execute
 *    - argument: arguments passed to task function
 *    - callback after execution is done:
 *       + input: result of task function
 *       + return: void
 * return:
 *    + std:future<void> => future proof, to check if task is finished or not, actual result will be delivered by callback
 *
 * Rational for callback: send loopback message to main thread
 */

namespace cpool {
class TaskManager
{
   public:
      TaskManager(unsigned int size = 0);
      ~TaskManager();

      void start();
      void stop();

      /**
       *
       */
      template<class FuncType, class... ArgsType>
      auto dispatch(FuncType&& function, ArgsType&&... args)
         -> std::future<typename std::result_of<FuncType(ArgsType...)>::type>
      {
         using ReturnType = typename std::result_of<FuncType(ArgsType...)>::type;

         if (function == nullptr)
         {
            // No point in setting up null task
            // return invalid future object, can be verified via 'valid()' function
            return {};
         }

         // Type erasor: remove FuncType, ArgsType from the function object
         auto newTask = std::make_shared<std::packaged_task<ReturnType()>>(
            [&] () -> ReturnType
            {
               // result will be delivered to the std::future object upon return
               return invoke(function, args...);
            }
         );

         auto taskResult = newTask->get_future();
         pushTask(newTask);
         return taskResult;
      }

      template<class FuncType, class CallbackType, class... ArgsType>
      auto dispatchCallback(FuncType&& function, CallbackType&& callback, ArgsType&&... args)
         -> typename std::enable_if<
               !std::is_member_pointer<CallbackType>::value,
               std::future<void>
            >::type
      {
         if (function == nullptr || callback == nullptr)
         {
            // No point in setting up null task
            // return invalid future object, can be verified via 'valid()' function
            return {};
         }

         // Type erasor: remove FuncType, ArgsType from the function object
         auto newTask = std::make_shared<std::packaged_task<void()>>(
            [&] () -> void
            {
               auto result = invoke(function, args...);
               callback(result);
               return;
            }
         );

         auto taskResult = newTask->get_future();
         pushTask(newTask);
         return taskResult;
      }


      // for cases of callback is a member function,
      // in which the first param will be the caller's instance
      template<class FuncType, class CallbackType, class CallerType, class... ArgsType>
      auto dispatchCallback(FuncType&& function, CallbackType&& callback, CallerType&& instance, ArgsType&&... args)
         -> typename std::enable_if<
               (!std::is_member_pointer<FuncType>::value
                  && std::is_member_pointer<CallbackType>::value),
               std::future<void>
            >::type
      {
         if (function == nullptr || callback == nullptr)
         {
            // No point in setting up null task
            // return invalid future object, can be verified via 'valid()' function
            return {};
         }

         // Type erasor: remove FuncType, ArgsType from the function object
         auto newTask = std::make_shared<std::packaged_task<void()>>(
            [&] () -> void
            {
               auto result = invoke(function, args...);
               invoke(callback, instance, result);
               return;
            }
         );

         auto taskResult = newTask->get_future();
         pushTask(newTask);
         return taskResult;
      }

      template<class FuncType, class CallbackType, class CallerType, class... ArgsType>
      auto dispatchCallback(FuncType&& function, CallbackType&& callback, CallerType&& instance, ArgsType&&... args)
         -> typename std::enable_if<
               (std::is_member_pointer<FuncType>::value
                  && std::is_member_pointer<CallbackType>::value),
               std::future<void>
            >::type
      {
         if (function == nullptr || callback == nullptr)
         {
            // No point in setting up null task
            // return invalid future object, can be verified via 'valid()' function
            return {};
         }

         // Type erasor: remove FuncType, ArgsType from the function object
         auto newTask = std::make_shared<std::packaged_task<void()>>(
            [&] () -> void
            {
               auto result = invoke(function, instance, args...);
               invoke(callback, instance, result);
               return;
            }
         );

         auto taskResult = newTask->get_future();
         pushTask(newTask);
         return taskResult;
      }
   private:
      std::atomic_bool isRunning;
      unsigned int poolSize;
      std::queue<std::function<void()>> tasks;
      std::vector<std::thread> workers;

      std::mutex taskMutex;
      std::condition_variable taskCond;

      template<class PackagedTask>
      void pushTask(PackagedTask&& newTask)
      {
         auto taskLock = std::unique_lock<std::mutex> {taskMutex};
         // Type erasor: remove ReturnType from the function object
         tasks.emplace(
            [newTask]()->void
            {
               (*newTask)();
            }
         );
         taskCond.notify_one();
      }

      /**
       * @brief Invoke a function object, which may be either a member pointer or a
       * function object. The first parameter will tell which.
       * (refer https://en.cppreference.com/w/cpp/utility/functional/invoke)
       */
      template<typename FuncType, typename... ArgsTypes>
      inline
      typename std::enable_if<
         (!std::is_member_pointer<FuncType>::value
            && !std::is_function<FuncType>::value
            && !std::is_function<typename std::remove_pointer<FuncType>::type>::value),
         typename std::result_of<FuncType&(ArgsTypes&&...)>::type
         >::type
      invoke(FuncType& f, ArgsTypes&&... args)
      {
         return f(std::forward<ArgsTypes>(args)...);
      }

      template<typename FuncType, typename... ArgsTypes>
      inline
      typename std::enable_if<
               (std::is_member_pointer<FuncType>::value
               && !std::is_function<FuncType>::value
               && !std::is_function<typename std::remove_pointer<FuncType>::type>::value),
               typename std::result_of<FuncType(ArgsTypes&&...)>::type
            >::type
      invoke(FuncType& f, ArgsTypes&&... args)
      {
         return std::mem_fn(f)(std::forward<ArgsTypes>(args)...);
      }

      // To pick up function references (that will become function pointers)
      template<typename FuncType, typename... ArgsTypes>
      inline
      typename std::enable_if<
         (std::is_pointer<FuncType>::value
            && std::is_function<typename std::remove_pointer<FuncType>::type>::value),
         typename std::result_of<FuncType(ArgsTypes&&...)>::type
         >::type
      invoke(FuncType f, ArgsTypes&&... args)
      {
         return f(std::forward<ArgsTypes>(args)...);
      }
};
}

#endif

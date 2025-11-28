#include <iostream>
#include <string>

#include "cpool_task.hpp"

class Result
{
   public:
      std::string value;
};

class Component
{
   public:
      Result taskFunc(int a, std::string b)
      {
         std::cout << "taskFunc: "
                  << a << ", " << b
                  << std::endl;
         Result ret;
         ret.value = "result";
         return ret;
      }


      void callback(Result ret)
      {
         std::cout << "Callback Result: "
                  << ret.value
                  << std::endl;
      }
};
int main(int argc, char const* argv [])
{
   unsigned int n = std::thread::hardware_concurrency();
   std::cout << n << " concurrent threads are supported.\n";

   Component caller;
   cpool::TaskManager pool;
   pool.start();

   std::cout  << "\n=======================================\n"
              << "Non-member function\n"
              << "=======================================\n";
   {
      // Non-member function
      auto ret = pool.dispatch([] (int a) -> int {
            std::cout << std::this_thread::get_id()
                     << " Work value = " << a
                     << std::endl;
            return 2;
         }, 1);

      ret.wait();

      std::cout << std::this_thread::get_id()
               << " RESULT = " << ret.get()
               << std::endl;
   }

   std::cout  << "\n=======================================\n"
            << "Member function\n"
            << "=======================================\n";
   {
      // Member function
      auto ret = pool.dispatch(&Component::taskFunc, caller, 1, "test");

      ret.wait();
      std::cout << std::this_thread::get_id()
               << " RESULT = " << ret.get().value
               << std::endl;
   }


   std::cout  << "\n=======================================\n"
              << "function & callback non member function\n"
              << "=======================================\n";

   {
      // callback non member
      auto callback = [](int v) {
            std::cout << std::this_thread::get_id()
                     << " Callback RESULT = " << v
                     << std::endl;
      };
      auto ret = pool.dispatchCallback([] (int a) -> int {
            std::cout << std::this_thread::get_id()
                     << " Work value = " << a
                     << std::endl;
            return 4;
         }, callback, 3);

      ret.wait();
   }

   std::cout  << "\n=======================================\n"
              << "function & callback member function\n"
              << "=======================================\n";

   {
      // callback non member
      auto ret = pool.dispatchCallback(&Component::taskFunc, &Component::callback, caller, 1, "test");

      ret.wait();
   }

   std::cout  << "\n=======================================\n"
              << "function member function\n"
              << "callback non-member function\n"
              << "=======================================\n";

   {

      // callback non member
      auto callback = [](Result v) {
            std::cout << std::this_thread::get_id()
                     << " Callback RESULT = " << v.value
                     << std::endl;
      };
      // callback non member
      auto ret = pool.dispatchCallback(&Component::taskFunc, callback, caller, 1, "test");

      ret.wait();
   }

   std::cout  << "\n=======================================\n"
              << "function non-member function\n"
              << "callback member function\n"
              << "=======================================\n";

   {

      auto taskFunc = [](int a, std::string b) -> Result
      {
         std::cout << "taskFunc: "
                  << a << ", " << b
                  << std::endl;
         Result ret;
         ret.value = "result";
         return ret;
      };
      auto ret = pool.dispatchCallback(taskFunc, &Component::callback, caller, 1, "test");

      ret.wait();
   }

   std::cout  << "\n=======================================\n"
              << "Invalid Non-member function\n"
              << "=======================================\n";
   {
      // Non-member function
      void (*cbfn)(Result) = NULL;
      auto ret = pool.dispatchCallback(&Component::taskFunc, cbfn, caller, 1, "test");

      if (ret.valid())
      {
         ret.wait();
      }
      else
      {
         std::cout << "Invalid dispatch call\n";
      }
   }

   pool.stop();
   return 0;
}

#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

#include "safequeue.h"
using namespace std;
class threadpool
{
private:
  class ThreadWorker {
  private:
    int m_id;
    threadpool * m_pool;
  public:
    ThreadWorker(threadpool * pool, const int id)
      : m_id(id), m_pool(pool) {
    }

    void operator()() {
      std::function<void()> func;
      bool dequeued;
      while (!m_pool->m_shutdown) {
        {
          std::unique_lock<std::mutex> lock(m_pool->m_conditional_mutex);
          if (m_pool->m_queue.empty()) {
            m_pool->m_conditional_lock.wait(lock);
          }
          dequeued = m_pool->m_queue.dequeue(func);
        }
        if (dequeued) {
          func();
        }
      }
    }
  };
 public:
      bool m_shutdown;
      SafeQueue<std::function<void()>> m_queue;
      std::vector<std::thread> m_threads;
      std::mutex m_conditional_mutex;
      std::condition_variable m_conditional_lock;
public:
  threadpool(const int n_threads)
    : m_shutdown(false),m_threads(std::vector<std::thread>(n_threads)) {
  }

  threadpool(const threadpool &) = delete;
  threadpool(threadpool &&) = delete;

  threadpool & operator=(const threadpool &) = delete;
  threadpool & operator=(threadpool &&) = delete;

  // Inits thread pool
  void init() {
    for (std::size_t i = 0; i < m_threads.size(); ++i) {
      m_threads[i] = std::thread(ThreadWorker(this, i));
    }
  }

  // Waits until threads finish their current task and shutdowns the pool
  void shutdown() {
    m_shutdown = true;
    m_conditional_lock.notify_all();

    for (std::size_t i = 0; i < m_threads.size(); ++i) {
      if(m_threads[i].joinable()) {
        m_threads[i].join();
      }
    }
  }

  // Submit a function to be executed asynchronously by the pool
  template<typename F, typename Cla,typename... Args>
  auto submitClassFun(F&& f, Cla&& cl,Args&&... args) -> std::future<decltype((cl->*f)(args...))> {
    // Create a function with bounded parameters ready to execute
    std::function<decltype((cl->*f)(args...))()> func = std::bind(std::forward<F>(f), std::forward<Cla>(cl),std::forward<Args>(args)...);
    // Encapsulate it into a shared ptr in order to be able to copy construct / assign
    auto task_ptr = std::make_shared<std::packaged_task<decltype((cl->*f)(args...))()>>(func);

    // Wrap packaged task into void function
    std::function<void()> wrapper_func = [task_ptr]() {
      (*task_ptr)();
    };

    // Enqueue generic wrapper function
    m_queue.enqueue(wrapper_func);

    // Wake up one thread if its waiting
    m_conditional_lock.notify_one();

    // Return future from promise
    return task_ptr->get_future();
  }

  template<typename F, typename...Args>
   auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
     // Create a function with bounded parameters ready to execute
     std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
     // Encapsulate it into a shared ptr in order to be able to copy construct / assign
     auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

     // Wrap packaged task into void function
     std::function<void()> wrapper_func = [task_ptr]() {
       (*task_ptr)();
     };

     // Enqueue generic wrapper function
     m_queue.enqueue(wrapper_func);

     // Wake up one thread if its waiting
     m_conditional_lock.notify_one();

     // Return future from promise
     return task_ptr->get_future();
   }

};

#endif // THREADPOOL_H

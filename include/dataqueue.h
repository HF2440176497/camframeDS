#ifndef DATAQUEUE_H
#define DATAQUEUE_H
#include <condition_variable>
#include <mutex>
#include <queue>

#define MAX_QUEUE_SIZE 2 //队列缓存大小
#define WAIT_MS 50

template <typename T>
class dataQueue {
 private:
  std::queue<T> m_queue;//队列
  std::mutex m_mutex;//锁
  std::condition_variable cv;//条件变量

 public:
 //构造函数
  dataQueue() {}
  //拷贝构造函数，待定
  dataQueue(dataQueue& other) {
    // TODO:
  }

  ~dataQueue() {}

  //队列是否空
  bool empty() {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_queue.empty();
  }
  //队列大小
  int size() {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_queue.size();
  }

  //队列写入
  void setData(T& t) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (this->m_queue.size() < MAX_QUEUE_SIZE)
    {
      this->m_queue.push(t);
      cv.notify_one();
    }
    else
    {
      this->m_queue.pop();
      this->m_queue.push(t);
      cv.notify_one();
    }
  }

  //队列读取
  T getData(int WAIT_MS_=WAIT_MS) {
    T data;
    std::unique_lock<std::mutex> lck(m_mutex);
    if(cv.wait_for(lck, std::chrono::milliseconds(WAIT_MS_),[this] { return !m_queue.empty(); })) // wait WAIT_MS 毫秒
    {
        data = std::move(this->m_queue.front());
        this->m_queue.pop();
        return data;
    }
    else
    {
        return data;
    }
  }
};

template <typename T>
struct transPtr {
  dataQueue<T> que;
  std::string params;
};
#endif  // DATAQUEUE_H



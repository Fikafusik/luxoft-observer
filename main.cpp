#include <condition_variable>
#include <iostream>
#include <list>
#include <queue>
#include <thread>
#include <set>

#include <stdlib.h>
#include <time.h>

#include "plog/Log.h"
#include "plog/Initializers/RollingFileInitializer.h"

/*
  [x] REQ_HW_0501. Приложение должно быть написано с использование языка программирования стандарта C++14.
  
  [x] REQ_HW_0502. Приложение должно поддерживать произвольное количество подписчиков.

  [x] REQ_HW_0503. Количество подписчиков должно быть определено первым аргументом коммандной строки.
    subscribers_count = atoi(argv[1]);
  
  [x] REQ_HW_0504. Подписчики не должны иметь доступ к переменным, которыми оперирует поток-производитель событий.
  
  [x] REQ_HW_0505. Подписчику должен быть присвоен уникальный индентификатор.
    Используется статическая переменная s_id, увеличивающаяся при каждом обращении к ней, что 
    гарантирует уникальность получаемых идентификторов. Подписчик получает идентификатор при создании.

  [x] REQ_HW_0506. Подписчик должен логировать в консоль, каждое событие инициированое издателем.

  [x] REQ_HW_0507. Логи подписчика должны содержать уникальный индентификатор подписчика.
    std::string subscriber_t::to_string() { return "subscriber[" + std::to_string(m_id) + "]"; }

  [x] REQ_HW_0508. Издатель должен логировать каждое изменение списка подписчиков (добавление/удаление).
    PLOG(plog::info) << subscriber.to_string() << " subcribed";
    PLOG(plog::info) << subscriber.to_string() << " unsubcribed";

  [ ] REQ_HW_0509. Издатель должен иметь доступ к переменным, которыми оперирует поток-производитель событий.

  [x] REQ_HW_0510. Приложение должно завершить свою работу через количество циклов событий равное двухкратному количеству подписчиков.
    int max_loop = 2 * subscribers_count;

  [x] REQ_HW_0511. Приложение должно завершить работу корректно, все выделенные ресурсы должны быть очищены.

*/

enum class event_t {
  data_produced, data_consumed
};

class subscriber_t {
public:
  subscriber_t(event_t interested_event_type) 
  : m_id(generate_id())
  , m_interested_event_type(interested_event_type) {
  }

  const event_t& get_interesed_event_type() const {
    return m_interested_event_type;
  }

  std::string to_string() const {
    return "subscriber[" + std::to_string(m_id) + "]";
  }

  virtual void on_notification(const std::string& data) const {
    PLOG(plog::info) << to_string() + ": " + data;
  }

  bool operator<(const subscriber_t& other) const 
  {
    return m_id < other.m_id;  //assume that you compare the record based on a
  }
private:
  static int generate_id() {
    return s_id++;
  }

  static int s_id;
private:
  int m_id;
  event_t m_interested_event_type;
};
int subscriber_t::s_id = 0;

class data_produced_subscriber_t : public subscriber_t {
public:
  data_produced_subscriber_t() 
  : subscriber_t(event_t::data_produced) {
  }

  void on_notification(const std::string& data) const override {
    PLOG(plog::info) << subscriber_t::to_string() + " data produced: " + data;
  }
private:
};

class data_consumed_subscriber_t : public subscriber_t {
public:
  data_consumed_subscriber_t() 
  : subscriber_t(event_t::data_consumed) {
  }

  void on_notification(const std::string& data) const override {
    PLOG(plog::info) << subscriber_t::to_string() + " data consumed: " + data;
  }
private:
};

class publisher_t {
public:
  void subscribe(subscriber_t* subscriber) {
    PLOG(plog::info) << subscriber->to_string() << " subcribed";
    m_subscribers.insert(subscriber);
  }

  void unsubscribe(subscriber_t* subscriber) {
    PLOG(plog::info) << subscriber->to_string() << " unsubcribed";
    m_subscribers.erase(subscriber);
  }

  void notify_all(event_t event, const std::string& data) {
    for (const auto& subscriber : m_subscribers) {
      // notify only interested in specific event type subscribers
      if (subscriber->get_interesed_event_type() == event) {
        subscriber->on_notification(data);
      }
    }
  }

  ~publisher_t() {
    while (!m_subscribers.empty()) {
      unsubscribe(*m_subscribers.cbegin());
    }
  }
private:
  std::set<subscriber_t *> m_subscribers;
};

constexpr int k_max_loop = 10;
constexpr int default_subscribers_count = 1;

// Compiler note: the 'pthread' library shall be linked
int main(int argc, char *argv[])
{
  plog::init(plog::debug, "Observer.txt");

  std::mutex mutex;
  std::condition_variable cv;
  std::queue<int> data;

  srand(time(NULL));
  
  int data_produced_subscribers_count = default_subscribers_count;
  int data_consumed_subscribers_count = default_subscribers_count;

  if (argc > 1) {
    data_produced_subscribers_count = atoi(argv[1]);
  }

  if (argc > 2) {
    data_consumed_subscribers_count = atoi(argv[2]);
  }

  publisher_t publisher;
  std::vector<subscriber_t *> subscribers;

  for (int i = 0; i < data_produced_subscribers_count; ++i) {
    subscribers.push_back(new data_produced_subscriber_t());
  }

  for (int i = 0; i < data_consumed_subscribers_count; ++i) {
    subscribers.push_back(new data_consumed_subscriber_t());
  }
  
  for (const auto& subscriber : subscribers) {
    publisher.subscribe(subscriber);
  }

  int max_loop = 2 * (data_produced_subscribers_count + data_consumed_subscribers_count);
  
  // Events producer - each one second generate random number with range 0..99
  // Syncronization complete via condition variable
  auto worker = std::thread([&publisher, &data, &mutex, &cv, max_loop]() {
    for(int i = max_loop; i; --i){
      std::this_thread::sleep_for(std::chrono::seconds(1U));
      int newValue = rand() % 100;
      {
        std::lock_guard<std::mutex> const lock(mutex);
        data.push(newValue);
        publisher.notify_all(event_t::data_produced, std::to_string(newValue));
        cv.notify_one();
      }
    };
  });

  // Events consumer
  while(max_loop){
    std::list<int> buffer;
    {
      std::unique_lock<std::mutex> lock(mutex);
      cv.wait(lock, [&data]() {return (data.size() != 0U); } );
      while(data.size()){
        int oldValue = data.front();
        buffer.push_back(oldValue);
        publisher.notify_all(event_t::data_consumed, std::to_string(oldValue));
        data.pop();
      }
    }
    for (auto const& iter : buffer){
      std::cout << "Value from queue - " << iter << std::endl;  
    }
    --max_loop;
  }

  worker.join();

  return EXIT_SUCCESS;
}

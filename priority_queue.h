#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include<list>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <set>
#include <map>
#include <shared_mutex>

#include "common.h"

//Очередь для обмена сообщениями с приоритетом  на основе std::multiset
//Использует mutex и contidional_variable для блокирования доступа
//очередь сделана синглтоном, чтоб все потоки точно работали гарантированно с одной и той же сущностью
namespace FastPriorityQueue
{

using namespace KBL;


//Обертка над сообщением, для корректного размещения его в мультисет
//Сравнение на основе строки совмещающей приоритет и номер сообщения поставит сообщения в нужном нам порядке
class MessageWrapper
{
    MSGptr m_data;
    std::string m_unique_str;


public:
    MessageWrapper(MSGptr msg):m_data(msg)
    {
        m_unique_str=std::to_string(m_data->priority)+std::to_string(m_data->msg_id);
    }

    std::string addr() const {return m_data->addr;}
    int64_t id() const {return m_data->msg_id;}


    bool operator==(const MessageWrapper& other) const
    {
        return m_unique_str==other.m_unique_str;
    }

    bool operator<(const MessageWrapper& other) const
    {
        return m_unique_str<other.m_unique_str;
    }

    //забрать данные
    MSGptr get_data() const {return m_data;}

};

//Синглтон для хранения сообщений, получивших подтверждение
//Для быстроты доступа используем std::set для многопоточности юзаем мьютексы
class PausedAddrKeeper
{

    std::mutex m_mutex2;
    std::set<int64_t> m_confirmed_set; //по id сообщения , наличие здесь означает подтверждение

    PausedAddrKeeper(){std::cout<<"Start PausedAddrKeeper\n";}
    ~PausedAddrKeeper(){}
    PausedAddrKeeper(const PausedAddrKeeper& ){}
    PausedAddrKeeper& operator=(const PausedAddrKeeper& ){}
public:
    static PausedAddrKeeper& get_instance()
    {
        static PausedAddrKeeper instance;
        return instance;
    }

    void set_confirmed(int64_t id)
    {
        if (id==0) return;
        std::lock_guard<std::mutex> lock1(m_mutex2);
        m_confirmed_set.insert(id);
    }

    bool is_confirmed(int64_t id)
    {
        std::lock_guard<std::mutex> lock1(m_mutex2);
        auto it=m_confirmed_set.find(id);
        bool res=(it!=m_confirmed_set.end());
        if (res) m_confirmed_set.erase(it);
        return res;
    }

};


//Очередь с приоритетом
//Содержит резервное хранилище для команд аресованных временно отключенным клиентам
class MyPriorityQueue
{
    using WrapType=MessageWrapper;
    std::multiset<WrapType> m_queue;
    std::map<std::string, std::list<MSGptr>> m_reserve;

    MyPriorityQueue(){std::cout<<"Start MyPriorityQueue\n";}
    ~MyPriorityQueue(){}
    MyPriorityQueue(const MyPriorityQueue& ){}
    MyPriorityQueue& operator=(const MyPriorityQueue& ){}
public:
    std::mutex mutex;
    std::mutex mutex_reserve;
    std::condition_variable cv;
    int highLoad=0; //количество потоков вставших на ожидание на объекте cv


    bool has_work() //Проверка есть ли работа для обработчиков
    {
        if (m_queue.empty()) return false;
        else return true;
    }

    //Помещаем команду в резервное хранилище команд
    void add_reserve(MSGptr msg)
    {
        std::lock_guard<std::mutex> lock(mutex_reserve);
        auto it=m_reserve.find(msg->addr);
        if (it!=m_reserve.end()) // для этого логина уже есть лист
        {
            (*it).second.push_back(msg);
        }
        else //Нет листа для этого логина
        {
            m_reserve[msg->addr]={msg};
        }
    }

    //Восстанавливаем команды из резервного хранилища обратно в очередь (для заданного адреса)
    void recovery(std::string addr)
    {
        std::unique_lock<std::mutex> lock(mutex_reserve);
        std::list<MSGptr> list;
        auto it=m_reserve.find(addr);
        if (it!=m_reserve.end()) // для этого логина уже есть лист
        {
            list=m_reserve[addr];
            m_reserve.erase(it);
        }
        else return;
        lock.unlock();
        std::lock_guard<std::mutex> lock1(mutex);
        for (auto msg: list) m_queue.insert(MessageWrapper(msg));

    }


    MSGptr get()
    {
        if (m_queue.empty()) return nullptr;
        highLoad--;
        auto it=m_queue.rbegin();
        auto res=it->get_data();
        m_queue.erase(--it.base());
        return res;
    }

    void add(MSGptr msg)
    {    
        std::lock_guard<std::mutex> lock(mutex);
        m_queue.insert(MessageWrapper(msg));
        highLoad++;
    }

    static MyPriorityQueue& get_instance()
    {
        static MyPriorityQueue instance;
        return instance;
    }


};



}; //end namespace

#endif // PRIORITY_QUEUE_H

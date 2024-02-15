#ifndef EXECUTORTHREAD_H
#define EXECUTORTHREAD_H
#include <thread>
#include <vector>
#include <list>
#include <memory>
#include <iostream>

#include "logger.h"
#include "priority_queue.h"
#include "commands.h"

using namespace FastPriorityQueue;
using namespace KBLLogger;



//Поток, обработчик исходных сообщений.
class ExecutorThread
{
    std::list<std::thread> m_threads;
    std::shared_ptr<StateFactory> m_factory;

    WaitingList& m_waitlist;


    void work()
    {
        //поток будет ждать работы на condition_variable из очереди priority_queue
        try {
            auto& queue=MyPriorityQueue::get_instance();
            auto func=[&](std::unique_lock<std::mutex>& lock) //функция которая будет вызываться при наличии данных в очереди
            {       
                MSGptr msg_ptr=queue.get();
                msg_ptr->highload=(queue.highLoad>255)? 255 : queue.highLoad; //помещаем в сообщение текущий уровень загрузки сервера
                lock.unlock();
                if (queue.has_work()) queue.cv.notify_one();
                if (msg_ptr)
                {
                    auto executor=m_factory->create(msg_ptr->command);
                    executor->execute(m_waitlist, msg_ptr);
                }

            };

        //подключаемся к очереди
        for(;;)
        {
            std::unique_lock<std::mutex> lock1(queue.mutex);
            if (queue.has_work()) func(std::ref(lock1)); //если очередь не пуста, рано вставать на ожидание. работаем!
            else //очередь пуста, встаем на ожидание
            {
                if (stop_all_thread) break;
                else
                {
                    queue.cv.wait(lock1, [&queue]{return (queue.has_work());});
                    func(std::ref(lock1));
                }
            }

        }

        }  catch (...) {
            std::cout<<"There is a big trouble: Unexpected exception in executor tread..."<<std::endl;
            KBL::Logger::Error("EXECUTOR ERR");

        }

    }

public:
    ExecutorThread(WaitingList& waitlist, size_t n=1) :  m_factory(new StateFactory), m_waitlist(waitlist)
    {
        std::cout<<"Start ExecutorThread\n";

        //Инициализация factory
        typedef int DummyType;
        m_factory->registrate<State_Creator<Send_with_answer,DummyType>, DummyType> ("S_ANSWER", 0);
        m_factory->registrate<State_Creator<Send_with_answer,DummyType>, DummyType> ("S_STRONG", 0);
        m_factory->registrate<State_Creator<Send_without_answer,DummyType>, DummyType> ("S_WEAK", 0);
        m_factory->registrate<State_Creator<Login, DummyType>, DummyType> ("LOGIN", 0);
        m_factory->registrate<State_Creator<Send_without_answer,DummyType>, DummyType> ("ANSWER", 0);
        m_factory->registrate<State_Creator<Broadcast,DummyType>, DummyType> ("BROADCAST", 0);
        m_factory->registrate<State_Creator<Clients,DummyType>, DummyType> ("CLIENTS", 0);
        m_factory->registrate<State_Creator<Disconnect,DummyType>, DummyType> ("DISCONNECT", 0);


        m_factory->registrate<State_Creator<DummyCommand,DummyType>, DummyType> ("CONSOLE", 0);


        //Запуск потоков
        for (size_t i=0; i<n; i++)
        {
            m_threads.emplace_back(std::thread([this]{work();}));
            m_threads.back().detach();
        }
    }

    ~ExecutorThread()
    {
        std::cout<<"Destroy executor thread\n";
    }

};






#endif // EXECUTORTHREAD_H

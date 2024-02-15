#ifndef TIME_OBJECTS_H
#define TIME_OBJECTS_H

#include "server.h"


//Класс хранитель сообщений с прикрепленным таймером ожидания подтверждения
//При срабатывении таймера осуществляется проверка получения подтверждения
//если подтверждение не получено, то сообщение помещается в резервное хранилище
class TimerKeeper: public std::enable_shared_from_this<TimerKeeper>
{
    MSGptr m_msg;
    steady_timer m_timer;

public:
    TimerKeeper(MSGptr msg, io_context& context): m_msg(msg), m_timer(context, std::chrono::steady_clock::now()+std::chrono::milliseconds(waiting_time)){}
    void start()
    {
        auto self(shared_from_this());
        m_timer.async_wait([this,self](boost::system::error_code error) {
            if (!error)
            {
                auto& paused=PausedAddrKeeper::get_instance();
                if (!paused.is_confirmed(m_msg->msg_id))
                {
                    auto& queue=MyPriorityQueue::get_instance();
                    queue.add(m_msg);
                    queue.cv.notify_one(); //уведомляем обработчик команд

                }
            }
            else
            {
                std::cout<<"timer error#1\n"<<error.value()<<" "<<error.message()<<std::endl;
                KBL::Logger::Error("TIME OBJ ERR#1 ");
            }
        });
    }

};




//Объект для запуска TimeKeeper
class WaitingList
{

    io_context& m_context;
    steady_timer m_timer;

public:


WaitingList(io_context& context) : m_context(context), m_timer(m_context, std::chrono::steady_clock::now()+std::chrono::milliseconds(1000))
{
    std::cout<<"Start WaitingList\n";
}

~WaitingList(){}

//Добавить TimerKeeper для данной команды
void add(MSGptr msg)
{
    auto ptr=std::make_shared<TimerKeeper>(msg, m_context);
    ptr->start();
}

};



#endif // TIME_OBJECTS_H

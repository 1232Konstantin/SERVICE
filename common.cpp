#include "common.h"

KBL::Logger::Logger()
{
    std::cout<<"Start logger\n";
    m_fileName=prefix_logger+time_mark()+".log";
    m_errorfileName=prefix_logger+time_mark()+"_errors.log";

    m_logger_thread= std::thread([this]{this->work();});
    m_logger_thread.detach();

}

void KBL::Logger::add(std::string msg)
{
    std::unique_lock lock(m_mutex);
    m_list.push_back(msg);
    lock.unlock();
    m_cv.notify_one();
}

void KBL::Logger::err(std::string error_string)
{
    std::unique_lock lock(m_mutex);
    m_list.emplace_back("#"+error_string);
    lock.unlock();
    m_cv.notify_one();
}

//Функуция возвращающая строку дата+время
std::string KBL::Logger::time_mark()
{
    auto now=std::chrono::system_clock::now();
    auto now_c=std::chrono::system_clock::to_time_t(now);
    std::tm tm=*std::localtime(&now_c);
    std::stringstream ss;
    ss<<std::put_time(&tm,"%Y%m%d_%H%M%S");
    return ss.str();
}

void KBL::Logger::work()
{
    try {

                auto func=[&](std::unique_lock<std::mutex>& lock) //функция которая будет вызываться при наличии данных в очереди
                {
                    auto msg=m_list.front();
                    m_list.pop_front();
                    lock.unlock(); //отпускаем мьютекс, так как запись в файл длительный процесс

                    auto file=(msg[0]=='#')? m_errorfileName: m_fileName;

                    m_stream.open(file,std::ios_base::app);
                    m_stream<<time_mark()<<" "<<msg<<std::endl;
                    m_stream.close();
                };

            //подключаемся к очереди
            while (!stop_all_thread)
            {
                //std::cout<<"+\n";
                std::unique_lock<std::mutex> lock1(m_mutex);
                if (m_list.size()>0) func(std::ref(lock1)); //если очередь не пуста, рано вставать на ожидание. работаем!
                else //очередь пуста, встаем на ожидание
                {
                    m_cv.wait(lock1, [this]{return (m_list.size()>0);});
                    func(std::ref(lock1));
                }
            }

            }  catch (...) {
                std::cout<<"There is a big trouble: Unexpected exception in file tread..."<<std::endl;
            }
}


std::string KBL::Logger::msg_string(MSGptr msg)
{
    std::string res;
    if (msg->message_data.size()<150)
    {
    auto arrow=sizeof(HardwareMsgHeader)-1;
    while (arrow<msg->message_data.size()-1)
    {
        arrow+=1;
        res+=msg->message_data[arrow];
    }
    }
    else
    {
        res="some_message_data ("+std::to_string(msg->message_data.size())+" Bytes)";
    }
    return res;
}



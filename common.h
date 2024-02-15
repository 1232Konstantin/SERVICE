#ifndef COMMON_H
#define COMMON_H
#include <iostream>
#include <vector>
#include <memory>
#include <mutex>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <list>
#include <fstream>
#include <thread>
#include <condition_variable>

#include <boost/algorithm/string.hpp>




namespace KBL{

//Адрес папки для хранения файлов логов
inline static std::string prefix_logger="";

//Признак для остановки потоков обработчиков и логирования
inline static bool stop_all_thread=false;

//Приоритет сообщения
enum PRIORITY{
  CASUAL=0,
  IMPORTANT,
  CRITICAL
};


//размер буфера
inline static size_t MAXL=128;


//Порт на сервере
inline static short port=9000;

//Время ожидания подтверждения до момента признания адресата paused (мс)
inline static short waiting_time=50;


//класс для  выдачи уникальных номеров
class Numerator
{
    std::mutex mutex;
    int64_t m_number=0;

    Numerator()=default;
    ~Numerator()=default;
    Numerator(const Numerator& ){}
    Numerator& operator=(const Numerator& ){}
public:
    int64_t get_unique_number()
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto res=++m_number;
        return res;
    }

    static Numerator& get_instance()
    {
        static Numerator instance;
        return instance;
    }
};



//Класс для транспортировки сообщений внутри программы
struct KBLMessage
{
    KBLMessage()=default;
    int64_t msg_id=0; //current id (is calculated before the command is placed in the priority queue)
    int64_t old_msg_id=0; //if command will be an answer old_msg_i will keep query message id
    int64_t source_id=0; // to identify source socket before login is known
    KBL::PRIORITY priority=KBL::PRIORITY::CASUAL;
    std::string command;// this is receiving value
    std::string addr;
    unsigned char highload=0; //текущий показатель загрузки сервера
    std::vector<char> message_data; //payload


    void operator=(KBLMessage& other)
    {
       old_msg_id=other.old_msg_id;
       msg_id=other.msg_id;
       source_id=other.source_id;
       priority=other.priority;
       command=other.command;
       addr=other.addr;
       message_data=other.message_data;
       highload=other.highload;
    }
    void operator=(KBLMessage&& other)
    {
        old_msg_id=other.old_msg_id;
        msg_id=other.msg_id;
        source_id=other.source_id;
        priority=other.priority;
        command=other.command;
        addr=other.addr;
        message_data=other.message_data;
        highload=other.highload;
    }
    KBLMessage(KBLMessage& other)
    {
        old_msg_id=other.old_msg_id;
        msg_id=other.msg_id;
        source_id=other.source_id;
        priority=other.priority;
        command=other.command;
        addr=other.addr;
        message_data=other.message_data;
        highload=other.highload;
    }

    //функция преобразования сообщений в строку, необходимая для логирования
    std::string print_log()
    {
        std::string res;
        res+="com: " +command+" ";
        res+="addr: " +addr+" ";
        res+="len: " +std::to_string(message_data.size())+" ";
        res+="id: " +std::to_string(msg_id)+" ";
        res+="responce: " +std::to_string(old_msg_id)+" ";
        res+="source: " +std::to_string(source_id)+" ";
        res+="priority: " +std::to_string(priority)+" ";
        return res;
    }
};


typedef   std::shared_ptr<KBLMessage> MSGptr;





//Класс заголовка с упаковкой побайтно
#pragma pack (push, 1)
class HardwareMsgHeader
{
public:
    HardwareMsgHeader()=default;

    char start='&';
    int64_t lenght;
    //int64_t old_id;
    int64_t id;
    unsigned char priority;
    char command[10]; //c-str format
    char addr[10]; //c-str format
    char highload;

   HardwareMsgHeader(MSGptr ptr)
   {
       lenght=ptr->message_data.size();
       id=ptr->msg_id;
       priority=(char)ptr->priority;
       highload=ptr->highload;

       for (unsigned int i=0; i<=ptr->command.length(); i++) command[i]=(i<ptr->command.length())? *(ptr->command.c_str()+i) :'\0';
       for (unsigned int i=0; i<=ptr->addr.length(); i++) addr[i]=(i<ptr->addr.length())? *(ptr->addr.c_str()+i) :'\0';
   }

   //Загрузка данных заголовка в MSGptr
   void fill_msg_ptr(MSGptr ptr)
   {
       ptr->msg_id=id;
       ptr->priority=(PRIORITY)priority;
       ptr->highload=highload;
       ptr->addr=std::string(addr);
       ptr->command=std::string(command);
   }


    void operator=(HardwareMsgHeader& other)
    {

       lenght=other.lenght;
       id=other.id;
       priority=other.priority;
       for (int i=0; i<10; i++)
       {
           command[i]=other.command[i];
           addr[i]=other.addr[i];
       }
    }
    void operator=(HardwareMsgHeader&& other)
    {
       lenght=other.lenght;
       id=other.id;
       priority=other.priority;
       for (int i=0; i<10; i++)
       {
           command[i]=other.command[i];
           addr[i]=other.addr[i];
       }
    }
    HardwareMsgHeader(HardwareMsgHeader& other)
    {

       lenght=other.lenght;
       id=other.id;
       priority=other.priority;
       for (int i=0; i<10; i++)
       {
           command[i]=other.command[i];
           addr[i]=other.addr[i];
       }
    }
};
#pragma pack(pop)

//Класс для логирования сообщений и ошибок (в разные файлы)
class Logger
{
    std::list<std::string> m_list;
    std::fstream m_stream;
    std::string m_fileName;
    std::string m_errorfileName;

    std::thread m_logger_thread;

    Logger();
    ~Logger(){}
    Logger(const Logger& ){}
    Logger& operator=(const Logger& ){}

    std::string time_mark();
    void work();

    std::string msg_string(MSGptr msg);
public:

    std::mutex m_mutex;
    std::condition_variable m_cv;

    static Logger& get_instance()
    {
        static Logger instance;
        return instance;
    }
    void add(std::string msg);
    void err(std::string error_string);


    static void Log(std::string msg)
    {
        auto& logger=Logger::get_instance();
        logger.add(msg);
    }
    static void Error(std::string error_string)
    {
        auto& logger=Logger::get_instance();
        logger.err(error_string);
    }
};


}; //namespace KBL

#endif // COMMON_H

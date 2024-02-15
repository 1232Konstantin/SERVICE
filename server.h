#ifndef SERVER_H
#define SERVER_H

#include <locale>
#include <boost/asio.hpp>
#include <vector>
#include <map>

#include <iostream>
#include <future>


#include <shared_mutex>

#include "priority_queue.h"
#include "circlebuffer.h"
#include <boost/system/error_code.hpp>
#include <boost/locale/encoding.hpp>
#include <boost/locale.hpp>


using namespace boost::asio;
using namespace KBL;
using namespace FastPriorityQueue;


//Класс хранящий сокет клиента, а также обеспечивающий операции чтения и записи
class Session: public std::enable_shared_from_this<Session>
{
    std::vector<char> m_v; //размер с запасом для заголовка
    char* m_data=nullptr;
    Analizer m_analizer;
    int64_t m_id;


public:
    std::atomic<bool> free=false; //состояние сессии, свободна или нет
    std::atomic<bool> not_loginning=false; //состояние валидный логин или нет
    ip::tcp::socket m_socket;

    Session(ip::tcp::socket socket, int64_t id) :m_id(id), m_socket(std::move(socket))
    {
        m_v.resize(MAXL);
        m_data=m_v.data();
    }


    void my_reset()
    {
        //free=true;
        m_analizer.reset();
    }

    void start() {
        std::cout<<"Server::Start session "<<m_id<<std::endl;
        free=false;
        doRead();
    }
    void doWrite(MSGptr msg);

    int64_t get_id() {return m_id;}

  private:

    void doRead();


};




class Server
{
    io_context& m_context;
    ip::tcp::acceptor acceptor_m;
    std::vector<std::thread> m_threads;
    size_t nThreads = std::thread::hardware_concurrency()/2;



    Server(const Server& s):m_context(s.m_context), acceptor_m(m_context, ip::tcp::endpoint(ip::tcp::v4(), port)){}
    Server& operator=(const Server& ){}

    //Получить объект session из пула
   std::shared_ptr<Session> get_session();

   void doAccept();

public:

    Server(io_context& context);
    ~Server(){}

};



//Sender объект ответственный за хранения пула сессий и отправку сообщений через них

class Sender
{
    std::shared_mutex m_mutex;
    std::map<int64_t, std::shared_ptr<Session>> m_pool;
    std::map<std::string, int64_t> m_logins;
    std::map<int64_t, std::string> m_logins_reverce;

    Sender(){}
    ~Sender(){}
    Sender(const Sender&){}
    Sender& operator=(const Sender& ){}

public:

    static Sender& get_instance()
    {
        static Sender instance;
        return instance;
    }

    //Получить объект session из пула
   std::shared_ptr<Session> get_session();


   //отправить данные заданному клиенту
   void send(MSGptr msg);

   //отправить данные заданному клиенту c ответом или подтверждением
   void send_with_answer(MSGptr msg);

   //Добавить объект session в пулл
   void add_session(std::shared_ptr<Session> ptr, int64_t unique_id);


   //отправить обратно в тот сокет откуда пришло сообщение
   void send_back(MSGptr msg);


    //отправить перечень клиентов по запросу
    void send_client_list(MSGptr msg);

    //Узнать логин по идентификатору
    std::string get_login_from_id(int64_t id);


    //зарегистрировать логин
    void login(MSGptr msg);


    //Отключить клиента, освободив объект session
    void disconnect(MSGptr msg);


    //Широковещательная рассылка сообщения
    void send_broadcast(MSGptr msg);

    //Удалить соответствие логина сессии
    void disconnect(std::shared_ptr<Session> session);

};




#endif // SERVER_H

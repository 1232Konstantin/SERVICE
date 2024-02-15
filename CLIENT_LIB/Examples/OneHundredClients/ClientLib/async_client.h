#ifndef ASYNCCLIENT_H
#define ASYNCCLIENT_H
#include <string>
#include <iostream>
#include <boost/asio.hpp>
#include "circlebuffer.h"
#include <memory>
#include <map>

using namespace boost::asio;

/*
 Класс ассисинхронного клиента работающий с сервисом сообщений.
 Для работы использует классификацию ошибок:
    OK - нет ошибки
    SENDING_ERR - ошибка отправки,
    CONNECTING_ABORT -отказ регистрации логина (вероятно логин занят),
    RECEIVING_ERR - ошибка приема,
    MSG_UNPACK_ERR - ошибка при распаковке сообщения (следует проверить сообщение на соответствие протоколу),
    UNDEFINED_ERR - неопознанная ошибка   

 При создании клиента в конструктор должен передаваться объект boost::asio::io_context, функтор обработки поступающих сообщений
 с сигнатурой void (MSGptr), функтор обработки ошибок ассинхронных операций с сигнатурой void (boost::system::error_code error),
 а также порт и ip адрес сервера сервиса. После создания клиент обеспечивает следующие операции:


1.  Подключение к сервису с желаемым логином (синхронная операция)
    ERR connect(std::string login);

2. Запрос перечня активных клиентов в системе (синхронная операция)
    std::vector<std::string> get_client_list(ERR& err);

3. Отключение клиента от сервиса (синхронная операция)
    ERR disconnect();

4. Последвотельный цикл ассинхронных операций чтения запускается методом start()


5. Ассинхронная от правка сообщения
    void assync_send(MSGptr msg)

6. Ассинхронная от правка сообщения с обязательным ответом
   void assync_send_with_answer(MSGptr msg, FUNCTOR answer_handler)

*/


namespace Async_Client
{
using namespace Circle;
inline static const std::string home="127.0.0.1";


//Класс клиента для взаимодействия с сервисом
class AsyncClient: public std::enable_shared_from_this<AsyncClient>
{
    typedef std::function<void (MSGptr, std::shared_ptr<AsyncClient>)> FUNCTOR;
    typedef std::function<void (boost::system::error_code)> ERROR_HANDLER;
    ip::tcp::socket m_socket;
    std::vector<char> m_v;
    char* m_data=nullptr;
    Analizer m_analizer;
    std::vector<char> m_dummy;
    FUNCTOR m_functor;
    ERROR_HANDLER m_err_handler;
    std::mutex m_mutex; //защитта answer_map
    std::map<int64_t, FUNCTOR> answer_map; //карта функторов для обработки ответа


    bool send_comm(std::string comm, std::string receiver, char* data, size_t size, PRIORITY p)
    {
        auto msg=std::shared_ptr<KBL::KBLMessage>( new KBL::KBLMessage());
        msg->priority=p;
        msg->command=comm;
        msg->addr=receiver;
        msg->message_data.resize(size+sizeof (HardwareMsgHeader)); //payload
        if (size!=0) memcpy(msg->message_data.data()+sizeof (HardwareMsgHeader), data, size);
        return send(msg)==ERR::OK;
    }

    ERR send(MSGptr msg)
     {
         HardwareMsgHeader temp_header(msg);
         memcpy(msg->message_data.data(), &temp_header, sizeof(HardwareMsgHeader)); //копируем заголовок в буфер сообщения
         boost::system::error_code error;
         write(m_socket, buffer(static_cast<void*>(msg->message_data.data()), msg->message_data.size()), error);
         if (!error) return ERR::OK;
         else return ERR::SENDING_ERR;
     }

    MSGptr receive(ERR &err)
    {
        while (true)
        {
           boost::system::error_code error;
           auto lenght=m_socket.read_some(buffer(m_data, MAXL), error);
           if (!error)
           {
               err=m_analizer.m_buf.add(m_data, lenght); //Помещаем в круговой буфер анализатора
               while (m_analizer.has_work())
               {
                   MSGptr msg=m_analizer.analize(err);
                   if (msg!=nullptr) return msg;
               }

           }
           else
           {
               err=ERR::RECEIVING_ERR;
               return nullptr;
           }
        }
    }

public:
    AsyncClient(boost::asio::io_context& io_context, std::string ip_addr, std::string port, FUNCTOR f, ERROR_HANDLER eh) : m_socket(io_context), m_functor(f), m_err_handler(eh)
    {
        m_v.resize(MAXL);
        m_data=m_v.data();

        ip::tcp::resolver resolver(io_context);
        boost::asio::connect(m_socket, resolver.resolve(string_view(ip_addr), string_view(port)));

    }

    //Старт последовательности ассинхронных операций чтения из сокета
    void start()
    {
        m_socket.async_read_some(buffer(m_data, MAXL),
                [this](boost::system::error_code error, int64_t lenght) {

                if (!error)
                {
                    m_analizer.m_buf.add(m_data, lenght);
                    while (m_analizer.has_work())
                    {
                        ERR err;
                        MSGptr msg=m_analizer.analize(err);
                        if (msg==nullptr) continue;

                        if (msg->command=="S_STRONG") //если нужно подтверждение отправим его автоматически
                        {
                            msg->command="CONFIRM";
                            async_send(msg);
                        }

                        if (msg->command=="ANSWER")
                        {
                            std::lock_guard<std::mutex> guard(m_mutex);
                            auto it=answer_map.find(msg->msg_id);
                            if (it!=answer_map.end())
                            {
                                auto self(shared_from_this());
                                (*it).second(msg, self); //если ответ ожидается, выполняем соответствующий функтор
                                answer_map.erase(it);
                            }
                        }
                        else
                        {
                            auto self(shared_from_this());
                            m_functor(msg, self); //обрабатываем сообщение
                        }
                    }
                }
                else
                {
                    if ((error==error::eof) || (error==error::connection_reset))
                    {
                        m_socket.close();
                    }
                    m_err_handler(error); //обрабатываем ошибку
                }
                 start();
        });
   }

    //Ассинхронная отправка сообщений
    void async_send(MSGptr msg)
    {
        HardwareMsgHeader temp_header(msg);
        memcpy(msg->message_data.data(), &temp_header, sizeof(HardwareMsgHeader)); //копируем заголовок в буфер сообщения
        async_write(m_socket, buffer(static_cast<void*>(msg->message_data.data()), msg->message_data.size()),
        [this](boost::system::error_code error, int64_t)
        {
            if (error) m_err_handler(error); //обрабатываем ошибку
        });

    }


    //асинхронно отправить сообщение с обязательным ответом и обработать ответ при получении
    void async_send_with_answer(MSGptr msg, FUNCTOR answer_handler)
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        answer_map[msg->msg_id]=answer_handler;
        async_send(msg);
    }


        //Синхронная операция регистрации логина должна выполняться до запуска start()
        Circle::ERR connect(std::string login)
        {
            auto res=send_comm("LOGIN", login, login.data(), login.size(), PRIORITY::CASUAL);
            if (!res) return ERR::SENDING_ERR;
            ERR err;
            auto msg=receive(err);
            if (!msg) return ERR::RECEIVING_ERR;

            if (msg->command=="CONFIRM") return ERR::OK;
            else return ERR::CONNECTING_ABORT;
        }



        //Синхронная операция
        Circle::ERR disconnect()
        {
            auto res=send_comm("DISCONNECT", "server", nullptr, 0, PRIORITY::CASUAL);
            boost::system::error_code error;
            m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
            m_socket.close();
            if ((!error) && (res)) return ERR::OK;
            else return ERR::UNDEFINED_ERR;
        }

        //синхронная операция запроса активных клиентов, должна применяться до запуска start()
        std::vector<std::string> get_client_list(ERR &err)
        {
            std::string musor="pl";
            std::vector<std::string> dummy{"#error"};
            auto res=send_comm("CLIENTS", "server", musor.data(), musor.size(), PRIORITY::CASUAL);
            if (!res)
            {
                err=ERR::SENDING_ERR;
                return dummy;
            }
            auto msg=receive(err);
            if (!msg)
            {
                err=ERR::RECEIVING_ERR;
                return dummy;
            }
            std::string str;
            auto arrow=sizeof(HardwareMsgHeader)-1;
            while (arrow<msg->message_data.size()-1)
            {
                arrow+=1;
                str+=msg->message_data[arrow];
            }
            std::vector<std::string> vector;
            boost::algorithm::split(vector, str, [](char ch){return (ch==' ');});
            return vector;
        }


};

}; //end namespace


#endif // ASYINCCLIENT_H

#ifndef CLIENT_H
#define CLIENT_H
#include <string>
#include <iostream>
#include <boost/asio.hpp>
#include "circlebuffer.h"
#include <memory>

using namespace boost::asio;

/*
 Класс синхронного клиента работающий с сервисом сообщений.
 Для работы использует классификацию ошибок:
    OK - нет ошибки
    SENDING_ERR - ошибка отправки,
    CONNECTING_ABORT -отказ регистрации логина (вероятно логин занят),
    RECEIVING_ERR - ошибка приема,
    MSG_UNPACK_ERR - ошибка при распаковке сообщения (следует проверить сообщение на соответствие протоколу),
    UNDEFINED_ERR - неопознанная ошибка

 При создании клиента в конструктор должен передаваться объект boost::asio::io_context,
 а также порт и ip адрес сервера сервиса. После создания клиент обеспечивает следующие операции:


1.  Подключение к сервису с желаемым логином
    ERR connect(std::string login);

2. Запрос перечня активных клиентов в системе
    std::vector<std::string> get_client_list(ERR& err);

3. Широковещательная отправка сообщения
    ERR send_broadcast(char* data, size_t size, PRIORITY p=CASUAL);

4. Отправка сообщения без гарантии доставки
    ERR send_weak(std::string receiver, char* data, size_t size, PRIORITY p=CASUAL);

5. Отправка сообщения с обязательным подтверждением получения /доставка гаратнтирована, даже если клиент временно не в сети /
    ERR send_strong(std::string receiver, char* data, size_t size, PRIORITY p=CASUAL);

6. Отправка сообщения с обязательным ответом
    std::vector<char> send_with_anser(std::string receiver, char* data, size_t size, ERR& err, PRIORITY p=CASUAL);

7. Синхронное получение сообщения (блокирующее ожидание)
    std::vector<char> receive_msg(ERR& err);

8. синхронно получить сообщение и если требуется подтверждение, подтвердить автоматически (блокирующее ожидание)
    std::vector<char> receive_msg_strong(ERR& err);

9. Отключение клиента от сервиса
    ERR disconnect();

10. Получение сообщения в виде указателя MSGptr (быстрее т.к. исключает копирование )
    MSGptr receive(ERR& err);

11. Отправкасообщения в виде указателя MSGptr
    ERR send(MSGptr msg);

*/


namespace Sync_Client
{
using namespace Circle;
inline static const std::string home="127.0.0.1";


//Класс клиента для взаимодействия с сервисом
class SyncClient
{
    ip::tcp::socket m_socket;
    std::vector<char> m_v; //размер с запасом для заголовка
    char* m_data=nullptr;
    Analizer m_analizer;
    std::vector<char> m_dummy;


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



public:
    SyncClient(boost::asio::io_context& io_context, std::string ip_addr, std::string port) : m_socket(io_context)
    {
        m_v.resize(MAXL);
        m_data=m_v.data();

        ip::tcp::resolver resolver(io_context);
        boost::asio::connect(m_socket, resolver.resolve(string_view(ip_addr), string_view(port)));

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

    Circle::ERR disconnect()
    {
        auto res=send_comm("DISCONNECT", "server", nullptr, 0, PRIORITY::CASUAL);
        boost::system::error_code error;
        m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
        m_socket.close();
        if ((!error) && (res)) return ERR::OK;
        else return ERR::UNDEFINED_ERR;
    }


    Circle::ERR send_broadcast(char* data, size_t size, PRIORITY p=PRIORITY::CASUAL)
    {
        return (send_comm("BROADCAST", "all", data, size, p))? ERR::OK : ERR::SENDING_ERR;
    }

    Circle::ERR send_weak(std::string receiver, char* data, size_t size, PRIORITY p=PRIORITY::CASUAL)
    {
        return (send_comm("S_WEAK", receiver, data, size, p))? ERR::OK : ERR::SENDING_ERR;
    }

    Circle::ERR send_strong(std::string receiver, char* data, size_t size, PRIORITY p=PRIORITY::CASUAL)
    {
        return (send_comm("S_STRONG", receiver, data, size, p))? ERR::OK : ERR::SENDING_ERR;
    }

    //отправка и ополучение ответа одним действием (блокирующее ожидание)
    std::vector<char> send_with_anser(std::string receiver, char* data, size_t size, ERR& err, PRIORITY p=PRIORITY::CASUAL)
    {
        auto res=send_comm("S_ANSWER", receiver, data, size, p);
        if (res) return receive_msg(err);
        else
        {
            return m_dummy;
            err=ERR::OK;
        }
    }

    //синхронно получить сообщение (блокирующее ожидание)
    std::vector<char> receive_msg(ERR &err)
    {
        std::vector<char> result;

        auto msg=receive(err);
        if (msg)
        {
            result.resize(msg->message_data.size()-sizeof (HardwareMsgHeader));
            memcpy(result.data(), msg->message_data.data()+sizeof(HardwareMsgHeader), result.size());
            return result;
        }
        else return m_dummy;
    }


    std::vector<char> receive_msg_strong(ERR & err)
    {
        std::vector<char> result;

        auto msg=receive(err);
        if (msg)
        {
            if (msg->command=="S_STRONG")
            {
                msg->command="CONFIRM";
                if (!send(msg)) err=ERR::SENDING_ERR;
            }

            result.resize(msg->message_data.size()-sizeof (HardwareMsgHeader));
            memcpy(result.data(), msg->message_data.data()+sizeof(HardwareMsgHeader)-1, result.size());
            return result;
        }
        else return m_dummy;
    }



};

}; //end namespace


#endif // CLIENT_H

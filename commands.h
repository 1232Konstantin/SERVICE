#ifndef COMMANDS_H
#define COMMANDS_H

#include "factory.h"



// Тут представлен набор объектов для обработки команд, приходящих по сети
// Типы команд:
// LOGIN- регистрация сокета с заданным логином
// ANSWER - ответ на заданное сообщение -->по сути это S_WEAK
// CONFIRM - подтверждение приема сообщения -->обработка этой команды должна происходить прям при поступлении на уровне сервера, совать ее в очередь контрпродуктивно
// S_ANSWER - послать и ждать ответа
// S_STRONG - послать с подтверждением получения
// S_WEAK - послать без подтверждения получения
// BROADCAST - послать всем без подтверждения получения
// CLIENTS - запросить перечень активных клиентов
// DISCONNECT - отключение клиента
// LOAD - запрос текущей загрузки сервера
// CONSOLE -перчатать содержимое на экран сервера (отладочная функция)




class Send_with_answer: public Base
{
public:
    Send_with_answer(int){}

    void execute(WaitingList& waitlist, MSGptr msg)final
     {
         //Отправляем
         auto& sender=Sender::get_instance();
         sender.send_with_answer(msg);

         //Помещаем в перечень ожидающих ответа
         waitlist.add(msg);
     }
     ~Send_with_answer()=default;
};



class Send_without_answer: public Base
{
public:
    Send_without_answer(int){}
    void execute(WaitingList& waitlist, MSGptr msg)final
     {
         //Отправляем
         auto& sender=Sender::get_instance();
         sender.send(msg);
     }
     ~Send_without_answer()=default;
};

class Login: public Base
{
public:
    Login(int){}
    void execute(WaitingList& waitlist,MSGptr msg)final
     {
        auto& sender=Sender::get_instance();
        sender.login(msg);
     }
     ~Login()=default;
};


class Broadcast: public Base
{
public:
    Broadcast(int){}
    void execute(WaitingList& waitlist, MSGptr msg)final
     {
         auto& sender=Sender::get_instance();
         sender.send_broadcast(msg);
     }
     ~Broadcast()=default;
};

class Clients: public Base
{
public:
    Clients(int){}
    void execute(WaitingList& waitlist, MSGptr msg)final
     {
         auto& sender=Sender::get_instance();
         sender.send_client_list(msg);
     }
     ~Clients()=default;
};

class Disconnect: public Base
{
public:
    Disconnect(int){}
    void execute(WaitingList& waitlist, MSGptr msg)final
     {
         auto& sender=Sender::get_instance();
         sender.disconnect(msg);
     }
     ~Disconnect()=default;
};


class DummyCommand: public Base
{
public:
    DummyCommand(int){}
    void execute(WaitingList& waitlist, MSGptr msg)final
     {
            std::cout<<msg->msg_id<<"("<<std::this_thread::get_id()<<")-> ";
            auto arrow=sizeof(HardwareMsgHeader)-1;
            while (arrow<msg->message_data.size()-1)
            {
                arrow+=1;
                std::cout<<msg->message_data[arrow];
            }
            std::cout<<std::endl;

     }
     ~DummyCommand()=default;
};

#endif // COMMANDS_H

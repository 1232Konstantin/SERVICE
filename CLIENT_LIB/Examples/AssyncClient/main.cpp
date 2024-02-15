#include <iostream>
#include "ClientLib/async_client.h"
#include <locale>
using namespace std;
using namespace Async_Client;


//запуск ассинхронного клиента с логином ААА c задачей отвечать на все запросы S_ANSWER


int main()
{
    setlocale(LC_ALL,"Russian");

    boost::asio::io_context io_context;

    auto errHandler=[](boost::system::error_code error)
    {
        std::cout<<"Error"<<error.value()<<error.message()<<std::endl;
    };


    auto msgHandler=[](MSGptr msg, std::shared_ptr<AsyncClient> client)
    {
        if (msg->command!="S_ANSWER") return;
        msg->command="ANSWER";
        client->async_send(msg);


        std::cout<<"RECEIVED:->";
        auto arrow=sizeof(HardwareMsgHeader)-1;
        while (arrow<msg->message_data.size()-1)
        {
            arrow+=1;
            std::cout<<msg->message_data[arrow];
        }
        std::cout<<std::endl;
    };


    auto client=std::make_shared<AsyncClient>(io_context, home, "9000",msgHandler, errHandler);

    std::string login="AAA";
    if (client->connect(login)!=ERR::OK) cout<<"Error_connection\n";
    else
    {
        cout<<"Connection ok\n";
        client->start();
        io_context.run();
    }


    char ch;
    cin>>ch;

    return 0;
}

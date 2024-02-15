#include <iostream>
#include "ClientLib/sync_client.h"
#include <locale>
using namespace std;
using namespace Sync_Client;


//запуск клиента с логином ААА c задачей отвечать на все запросы S_ANSWER
void test(SyncClient& client)
{
    std::string login="AAA";


    if (client.connect(login)!=ERR::OK) cout<<"Error_connection\n";
    else
    {
        std::cout<<"Connection ok\n";

        while (true)
        {
            ERR err;
            auto msg=client.receive(err);
            if (!msg) cout<<"ERROR1";

            msg->command="ANSWER";
            if (client.send(msg)!=ERR::OK) cout<<"ERROR2";


            std::cout<<"RECEIVED:->";
            auto arrow=sizeof(HardwareMsgHeader)-1;
            while (arrow<msg->message_data.size()-1)
            {
                arrow+=1;
                std::cout<<msg->message_data[arrow];
            }
            std::cout<<std::endl;

        }
    }
}






int main()
{
    setlocale(LC_ALL,"Russian");

    boost::asio::io_context io_context;

    auto client=SyncClient(io_context, home, "9000");

    test(client);


    char ch;
    cin>>ch;

    return 0;
}

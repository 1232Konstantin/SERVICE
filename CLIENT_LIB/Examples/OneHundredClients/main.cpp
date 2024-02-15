#include <iostream>
#include "ClientLib/sync_client.h"
#include <locale>
using namespace std;
using namespace Sync_Client;



int main()
{
    setlocale(LC_ALL,"Russian");

    boost::asio::io_context io_context;
    std::list<std::shared_ptr<SyncClient>> list;
    std::string login= "login";
    std::string msg="My_message";
    for (int j=0; j<5; j++)
    {

        for (int i=0; i<100; i++)
        {
            std::shared_ptr<SyncClient> client{new SyncClient(io_context, home, "9000")};
            if (client->connect(login+std::to_string(i))!=ERR::OK) cout<<"Error connection "<<i<<"\n";
            else cout<<"Connection "<<i<<"\n";
            list.push_back(client);
        }

        for (int i=0; i<50; i++)
        {
            for (auto x: list)
            {
                msg="My_message"+std::to_string(i);
                ERR err;
                auto v=x->send_with_anser("AAA",msg.data(), msg.size(), err);

            }
        }

        std::cout<<"disconnection\n";
        list.clear();
    }

    char ch;
    cin>>ch;
    return 0;
}




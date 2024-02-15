#include <iostream>
#include "executorthread.h"
using namespace std;

//Сервис обмена сообщениями см. описание в doc файле






//Функция обрабатывает конфигурационный файл server.config
//Формат файла:
/*

Log_files_path=D:\\_temp\\
Buffer_size=1024
Port=9000
Waiting_time=50

*/

bool config()
{
    bool result=true;
    std::fstream stream;
    stream.open("server.config",std::ios_base::in);
    if (!stream.is_open())
    {
        std::cout<<"Can't find config file\n";
        return false;
    }
    std::string str;

    auto f=[](std::string str, std::string header)->std::string
    {
        vector<std::string> vector;
        boost::algorithm::split(vector, str, [](char ch){return (ch=='=');});
        if (vector.front()==header) return vector.back();
        else return "error";
    };

        stream>>str;
        auto res=f(str, "Log_files_path");
        if (res!="error") prefix_logger=res;
        else result=false;

        stream>>str;
        res=f(str, "Buffer_size");
        if (res!="error") MAXL=std::stoi(res);
        else result=false;

        stream>>str;
        res=f(str, "Port");
        if (res!="error") port=std::stoi(res);
        else result=false;

        stream>>str;
        res=f(str, "Waiting_time");
        if (res!="error") waiting_time=std::stoi(res);
        else result=false;

    stream.close();
    return result;
}



int main()
{
    setlocale(LC_ALL,"Russian");


    cout << "Start message exchange service..." << endl;
    if (!config()) std::cout<<"Incorrect config file"<<endl;
    try
    {

        KBL::Logger::get_instance();

        io_context context;

        WaitingList waitinglist(context);


        auto nExecutorThreads = std::thread::hardware_concurrency()/2-1;
        ExecutorThread executor(waitinglist, nExecutorThreads);

        Server server(context);

    }  catch (std::exception e)
    {
        cout<<"Exception from Server:  "<<e.what()<<endl;
    }

    return 0;
}

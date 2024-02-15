
#include "server.h"



    void Session::doWrite(MSGptr msg)
    {
        //std::cout<<"doWrite "<<msg->print_log()<<"\n";
        if (free) return; //нет смысла пытаться передавать через отключенный Session
        auto self(shared_from_this());
        HardwareMsgHeader temp_header(msg);
        memcpy(msg->message_data.data(), &temp_header, sizeof(HardwareMsgHeader)); //копируем заголовок в буфер сообщения
        async_write(m_socket, buffer(static_cast<void*>(msg->message_data.data()), msg->message_data.size()),
        [this, self, msg](boost::system::error_code error, int64_t)
        {
            if (error)
            {

                //KBL::Logger::Error("SERVER WRITE ERROR: "+error.to_string()+" "+error.message());
                KBL::Logger::Error("SERVER WRITE ERROR: "+error.message());
                std::cout<<"Write Error:  "<<error.value()<<" "<<error.message()<<std::endl;
            }
            else
            {
                //Отправляем на логирование (в этот момент больше никто кроме логера не будет работать с сообщением, соответственно теперь гонки не будет)
                KBL::Logger::Log(msg->print_log());
            }
        });
    }



    void Session::doRead()
    {
        //std::cout<<"do_read";
        auto self(shared_from_this());
        m_socket.async_read_some(buffer(m_data, MAXL),
                [this, self](boost::system::error_code error, int64_t lenght) {

                if (!error)
                {

                    MyPriorityQueue& msg_queue=MyPriorityQueue::get_instance();
                    m_analizer.m_buf.add(m_data, lenght); //Помещаем в круговой буфер анализатора
                    while (m_analizer.has_work())
                    {
                        MSGptr msg=m_analizer.analize();
                        if (msg==nullptr) continue;
                        if (msg->command=="ANSWER") msg->old_msg_id=msg->msg_id; //запоминаем, чтобы в дальнейшем проинформировать клиента какой именно запрос отвечен

                        if ((msg->command=="CONFIRM") || (msg->command=="ANSWER"))
                        {
                            //Помечаем исходную команду как ответченную
                            auto& commlist= PausedAddrKeeper::get_instance();
                            commlist.set_confirmed(msg->msg_id);
                        }
                        if ((msg->command!="CONFIRM") && (msg->command!="LOAD"))
                        {
                            auto& numerator=Numerator::get_instance();
                            msg->msg_id=numerator.get_unique_number();

                            msg->source_id=m_id; //cохраняем в команде источник (socket id)
                            msg_queue.add(msg);
                            msg_queue.cv.notify_one(); //уведомляем обработчик команд
                        }
                        if (msg->command=="LOAD") //команда для балансировки нагрузки в случае нескольких серверов, нужен максимально мыстрый ответ
                        {
                            std::unique_lock<std::mutex> lock(msg_queue.mutex);
                            msg->highload=(msg_queue.highLoad>255)? 255 : msg_queue.highLoad;
                            msg->command="LOAD_ANS";
                            doWrite(msg);
                        }
                    }
                }
                else
                {
                    if ((error==error::eof) || (error==error::connection_reset))
                    {
                        m_socket.close();
                        free=true;
                        not_loginning=true;
                    }

                    //KBL::Logger::Error("SERV READ: "+error.to_string()+" "+error.message());
                    KBL::Logger::Error("SERV READ: "+error.message());
                    std::cout<<"ReadError:  "<<m_id<<" "<<free<<" "<<error.value()<<" "<<error.message()<<std::endl;
                }
                if (!free) doRead();
        });

    }




//*********************************** объект Server ***************************************


    Server::Server(io_context &context): m_context(context), acceptor_m(m_context, ip::tcp::endpoint(ip::tcp::v4(), port))
    {
        std::cout<<"Start server\n";

        acceptor_m.set_option(ip::tcp::acceptor::reuse_address(true));
        doAccept();

        //Use signal_set for terminate context
        signal_set signals{m_context, SIGINT, SIGTERM};
        //signal SIGINT will be send when user press Ctrl+C (terminate)
        //signal SIGTERM will be send when system terminate program
        signals.async_wait([&](auto, auto) {m_context.stop(); });

          m_threads.reserve(nThreads);
          for (unsigned int i = 0; i < nThreads; ++i)
          {
            m_threads.emplace_back([this]() { m_context.run(); });
          }
          for (auto &th : m_threads)
          {
            th.join(); //this is io_context run procedure into every thread from threads
            //that will allow us to use multiple threads to work on client's messages
          }
    }



    void Server::doAccept()
    {
        auto session=get_session();
        if (!session) return; //дополнительная проверка
        acceptor_m.async_accept(session->m_socket, [this, session](boost::system::error_code error) {
            if (!error)
           {
               session->start();
           }
           else
            {
                //KBL::Logger::Error("SERV ACCEPT: "+error.to_string()+ " "+error.message());
                KBL::Logger::Error("SERV ACCEPT: "+error.message());
                std::cout<<"ERR ACCEPT: "<<error.value()<<" "<<error.message()<<std::endl;
            }

           doAccept();
        });
    }


    std::shared_ptr<Session> Server::get_session()
    {
        auto& sender=Sender::get_instance();
        auto session=sender.get_session();
        if (session!=nullptr)
        {
            session->my_reset();
            return session;
        }
        else
        {
            auto& numerator=Numerator::get_instance();
            auto unique_id=numerator.get_unique_number();
            ip::tcp::socket socket(m_context);
            auto ptr=std::make_shared<Session>(std::move(socket),unique_id);
            sender.add_session(ptr, unique_id);
            return ptr;
        }
    }




 //*********************************** объект Sender ***************************************


   void Sender::disconnect(std::shared_ptr<Session> session)
    {
       std::unique_lock<std::shared_mutex> lock(m_mutex);
       auto id=session->get_id();
       auto login=m_logins_reverce[id];

       auto it1=m_logins.find(login);
       if (it1==m_logins.end()) return;
       auto it=m_pool.find(m_logins[login]);
       if (it==m_pool.end()) return;
       auto pair=*it;
       pair.second->free=true;
       pair.second->not_loginning=true;

       auto it3=m_logins_reverce.find(id);
       m_logins.erase(it1);
       m_logins_reverce.erase(it3);
    }



    //Получить объект session из пула
   std::shared_ptr<Session> Sender::get_session()
   {
       std::unique_lock<std::shared_mutex> lock(m_mutex);
       for (auto x: m_pool) if (x.second->free) return x.second;
       return nullptr;
   }


   //Добавить объект session в пулл
   void Sender::add_session(std::shared_ptr<Session> ptr, int64_t unique_id)
  {
      std::unique_lock<std::shared_mutex> lock(m_mutex);
      m_pool[unique_id]=ptr;
  }

   //отправить данные заданному клиенту
   void Sender::send(MSGptr msg)
   {
       std::shared_lock<std::shared_mutex> lock(m_mutex);
       auto it1=m_logins.find(msg->addr);
       if (it1==m_logins.end())  return; //если клиент с данным логином не активен

       auto it=m_pool.find(m_logins[msg->addr]);
       if (it==m_pool.end()) return;
       auto pair=*it;
       lock.unlock();
       if (pair.second->free) //проверим активность сессии если она свободна отсоединяем ее от логина
       {
           disconnect(pair.second);
           return;
       }
       if (msg->command=="ANSWER") msg->msg_id=msg->old_msg_id;
       pair.second->doWrite(msg);
   }

   //отправить данные заданному клиенту
   void Sender::send_with_answer(MSGptr msg)
   {
       std::shared_lock<std::shared_mutex> lock(m_mutex);
       auto it1=m_logins.find(msg->addr);
       if (it1==m_logins.end())  //если клиент с данным логином не активен
       {
           MyPriorityQueue& msg_queue=MyPriorityQueue::get_instance();
           msg_queue.add_reserve(msg);
           return;
       }
       auto it=m_pool.find(m_logins[msg->addr]);
       if (it==m_pool.end()) return;
       auto pair=*it;
       lock.unlock();
       if (pair.second->free) //проверим активность сессии если она свободна отсоединяем ее от логина
       {
           disconnect(pair.second);
           MyPriorityQueue& msg_queue=MyPriorityQueue::get_instance();
           msg_queue.add_reserve(msg);
           return;
       }

       msg->addr=m_logins_reverce[msg->source_id];  //в последний момент меняем адрес чтоб ответ доходил куда нужно
       pair.second->doWrite(msg);
   }

   //отправить обратно туда откуда пришло сообщение
   void Sender::send_back(MSGptr msg)
   {
       std::shared_lock<std::shared_mutex> lock(m_mutex);
       auto it=m_pool.find(msg->source_id);
       if (it==m_pool.end()) return;
       auto pair=*it;
       if (pair.second->free) //проверим активность сессии если она свободна отсоединяем ее от логина
       {
           disconnect(pair.second);
           return;
       }
       pair.second->doWrite(msg);
   }


    //отправить перечень клиентов по запросу
    void Sender::send_client_list(MSGptr msg)
    { 
        //std::cout<<"send_client_list:";
        std::string clientList;
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        for (auto x: m_logins) clientList+=" "+x.first;
        auto l=clientList.size()+1;
        msg->message_data.resize(l+sizeof (HardwareMsgHeader));
        memcpy(msg->message_data.data()+sizeof (HardwareMsgHeader), clientList.c_str(),l+1);
        send_back(msg);

    }

    //Узнать логин по идентификатору
    std::string Sender::get_login_from_id(int64_t id)
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_logins_reverce[id];
    }

    //зарегистрировать логин
    void Sender::login(MSGptr msg)
    {
        auto login=msg->addr;
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        auto it=m_logins.find(login);
        if (it!=m_logins.end())
        {
            if (!m_pool[(*it).second]->not_loginning)
            {
                lock.unlock();
                msg->command="ABORT";
                send_back(msg);
                return;
            }
        }
        m_logins[login]=msg->source_id;
        m_logins_reverce[msg->source_id]=login;
        m_pool[msg->source_id]->not_loginning=false; //логин подтвержден
        msg->command="CONFIRM";
        lock.unlock();
        MyPriorityQueue& msg_queue=MyPriorityQueue::get_instance();
        msg_queue.recovery(login);
        send_back(msg);
    }



    //Отключить клиента, освободив объект session
    void Sender::disconnect(MSGptr msg)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        auto it1=m_logins.find(msg->addr);
        if (it1==m_logins.end()) return;
        auto it=m_pool.find(m_logins[msg->addr]);
        if (it==m_pool.end()) return;
        auto pair=*it;
        pair.second->free=true;
        pair.second->not_loginning=true;

        auto it3=m_logins_reverce.find(m_logins[msg->addr]);
        m_logins.erase(it1);
        m_logins_reverce.erase(it3);
    }

    //Широковещательная рассылка сообщения
    void Sender::send_broadcast(MSGptr msg)
    {
        for (auto x: m_logins)
        {
            msg->addr=x.first;
            send(msg);
        }
    }





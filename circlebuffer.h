#ifndef CIRCLEBUFFER_H
#define CIRCLEBUFFER_H

#include "common.h"
using namespace KBL;



//Циклический буффер
class CircleBuffer
{
    int64_t CMAX=2*MAXL;
    std::vector<char> m_v;
    char* m_buf=nullptr;  //начало
    char* m_last=nullptr; //указатель на байт за границей буфера сразу за последним
    char* m_begin=nullptr; //указатель на начало полезных данных
    char* m_end=nullptr;  //указатель на следующий байт за последним байтом полезных данных
    int64_t m_size=0;


    void add_count_end(char* data, int64_t len)
    {
        int64_t temp=m_last-m_end; //так как оба сдвинуты на 1, то даст реальное количество!
        if (temp>=len)
        {
            memcpy(m_end, data,len);
            m_end=m_end+len;
        }
        else
        {
            int64_t ostatok=len-temp;
            memcpy(m_end, data,temp);
            memcpy(m_buf, data+temp, ostatok);
            m_end=m_buf+ostatok;
        }
    }

    void pop_count_begin(char* data, int64_t len)
    {
        if (m_begin<m_end)
        {
            if (m_begin+len>m_last)
            {

                KBL::Logger::Error("CIRCLE ERR#1 ");
                std::cout<<"Error into circleBuf\n";
                return;
            }
            memcpy(data, m_begin,len);
            m_begin=m_begin+len;
            if (m_begin==m_end) m_begin=m_end=m_buf; //если забрали все, то буфер вернется в исходное состояние
        }
        else
        {
            int64_t temp=m_last-m_begin; //правильное значение
            if (len<temp)
            {
                memcpy(data, m_begin,len);
                m_begin=m_begin+len;
            }
            else
            {
                int64_t ostatok=len-temp;
                memcpy(data, m_begin,temp);
                memcpy(data+temp, m_buf, ostatok);
                m_begin=m_buf+ostatok;
                if (m_begin==m_end) m_begin=m_end=m_buf; //если забрали все, то буфер вернется в исходное состояние
            }
        }

    }


public:
    CircleBuffer()
    {
        m_v.resize(CMAX);
        m_buf=m_v.data();
        m_last=m_buf+CMAX;
        m_begin=m_buf;
        m_end=m_buf;
    }
    void add(char* data, int64_t len)
    {
        if (len>CMAX) {
            KBL::Logger::Error("CIRCLE ERR#2 ");
            std::cout<<"To big size for circleBuf\n";
            return;
        }

        add_count_end(data, len);
        m_size=m_size+len;
        m_begin=(m_size>CMAX)? m_end : m_begin;
        m_begin=(m_begin==m_last)? m_buf : m_begin;
        m_size=(m_size>CMAX)? CMAX : m_size;
    }
    char* front() {return m_begin;}
    char get_front() {return *m_begin;}

    void pop_front(char* data, int64_t n=1) //переписываем n байт из начала буфера в заданный адрес
    {
        if (n>m_size) {
            KBL::Logger::Error("CIRCLE ERR#3 ");
            std::cout<<"To much for circleBuf\n";
            return;
        }
        pop_count_begin(data, n);
        m_size-=n;
        if (m_size==0)
        {
            m_begin=m_end=m_buf;
        }

    }

    void print_buf()
    {
        std::cout<<"@buffer@"<<(m_begin-m_buf)<<" "<<(m_end-m_buf)<<" "<<(m_size)<<"\n";
        std::vector<char> buf;
        buf.resize(CMAX);
        memcpy(buf.data(), m_buf, CMAX);
        for (auto ch : buf) std::cout<<ch;
        std::cout<<std::endl;

        if (m_begin<m_end)
        {
            for (int64_t i=0; i<CMAX; i++)
            {
                char ch=((i>=m_begin-m_buf)&&(i<=m_end-m_buf))? m_buf[i] : '_';
                std::cout<<ch;
            }
        }
        else
        {
            for (int64_t i=0; i<CMAX; i++)
            {
                char ch=((i>=m_begin-m_buf)||(i<=m_end-m_buf))? m_buf[i] : '_';
                std::cout<<ch;
            }
        }

        std::cout<<std::endl;
        std::cout<<std::endl;

    }

    int64_t size()
    {
        return m_size;
    }

    //сбросить данные в буффере
    void reset()
    {
        m_buf=m_v.data();
        m_last=m_buf+CMAX;
        m_begin=m_buf;
        m_end=m_buf;
    }

};

//Анализатор будет выделять из непрерывного потока данных команды в соответствии с заголовком HardwareMsgHeader
//на выходе будет прям готовый умный указатель на сообщение MSGptr
class Analizer
{
    bool m_start_state=true; //состояние анализатора (закачка заголовка/закачка данных)
    MSGptr m_result=nullptr;
    HardwareMsgHeader m_current_header;
    int64_t m_current_pice=0; //сколько байт paylod осталось скачать
    char* m_addr=0; //куда качать paylod
public:
    CircleBuffer m_buf;
    Analizer()=default;

    MSGptr analize()
    {
        auto bufsize=m_buf.size();
        if (m_start_state)
        {
            if (*(m_buf.front())!='&')
            {
                std::cout<<"Analizator Error\n"; //Проверяем что сообщение начинается с маркера
                KBL::Logger::Error("ANALIZER ERROR");
            }
            if (bufsize>=sizeof(HardwareMsgHeader))
            {
                   m_buf.pop_front(reinterpret_cast<char*>(&m_current_header), sizeof(HardwareMsgHeader));

                   m_start_state=false;
                   m_result=std::shared_ptr<KBLMessage>(new KBLMessage);
                   m_result->message_data.resize(m_current_header.lenght); //Размер с запасом для хранения заголовка
                   m_current_header.fill_msg_ptr(m_result);
                   m_addr=m_result->message_data.data()+sizeof(HardwareMsgHeader); //резервируем в начале место под заголовок
                   m_current_pice=m_current_header.lenght-sizeof(HardwareMsgHeader);
            }

        }
        else
        {
            if (bufsize<m_current_pice)
            {
                m_buf.pop_front(m_addr, bufsize);
                m_addr+=bufsize;
                m_current_pice-=bufsize;
            }
            else
            {
                m_buf.pop_front(m_addr, m_current_pice);
                m_start_state=true;
                return m_result;
            }
        }
        return nullptr;
    }

    bool has_work()
    {
        if (!m_start_state) return (m_buf.size()>0)? true: false;
        else return (m_buf.size()>sizeof(HardwareMsgHeader))? true: false;

    }

    void reset()
    {
        m_buf.reset();
        m_start_state=true;
    }

};



#endif // CIRCLEBUFFER_H

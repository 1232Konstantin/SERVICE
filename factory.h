#ifndef FACTORY_H
#define FACTORY_H
#include <memory>
#include <string>
#include <map>
#include <functional>
#include "common.h"
#include "time_objects.h"

//Библиотека фабрики.
//Не требует кастомизации при условии, что объект состояния создается из одного POD аргумента
using namespace KBL;

//Интерфейс для хранения объектов внутри фабрики
class Base
{
public:
    virtual void execute(WaitingList&, MSGptr)=0;
    virtual ~Base()=default;
};

//Абстрактный класс создателя
class Abstract_BaseState_Creator
{
public:
    virtual ~Abstract_BaseState_Creator() {}
    virtual std::unique_ptr<Base> create() const = 0;
};

//Шаблонный креатор для создания объектов
template <typename  State, typename U>       //Класс ChildObject наследник класса BaseState
class State_Creator: public Abstract_BaseState_Creator
{
    U m_arg;
public:
    State_Creator(const U arg) : m_arg(arg){}
    virtual std::unique_ptr<Base> create() const override {return std::unique_ptr<Base>(new State(m_arg));}
};

//Фабрика
class StateFactory
{
public:
 StateFactory () = default;

 using map_type= std::map<std::string, std::shared_ptr<Abstract_BaseState_Creator>> ;

 template<typename StateCreator,typename Arg>
    void registrate (const std::string& type, Arg arg) //добавление креатора объектов в map
    {
            map_type::iterator it = m_map.find(type);
            if (it == m_map.end())
                m_map[type] = std::shared_ptr<Abstract_BaseState_Creator>(new StateCreator(arg));
    }

    std::unique_ptr<Base> create(const std::string& type) //создание объекта
    {
            map_type::iterator it = m_map.find(type);
            if (it != m_map.end())   return it->second->create();
            return 0;
    }

private:
    map_type m_map;

};


#endif // FACTORY_H

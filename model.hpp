/* Copyright Vincent Lanore (2017) - vincent.lanore@gmail.com

This software is a computer program whose purpose is to provide the necessary classes to write
ligntweight component-based c++ applications.

This software is governed by the CeCILL license under French law and abiding by the rules of
distribution of free software. You can use, modify and/ or redistribute the software under the terms
of the CeCILL license as circulated by CEA, CNRS and INRIA at the following URL
"http://www.cecill.info".

As a counterpart to the access to the source code and rights to copy, modify and redistribute
granted by the license, users are provided only with a limited warranty and the software's author,
the holder of the economic rights, and the successive licensors have only limited liability.

In this respect, the user's attention is drawn to the risks associated with loading, using,
modifying and/or developing or reproducing the software by the user in light of its specific status
of free software, that may mean that it is complicated to manipulate, and that also therefore means
that it is reserved for developers and experienced professionals having in-depth computer knowledge.
Users are therefore encouraged to load and test the software's suitability as regards their
requirements in conditions enabling the security of their systems and/or data to be ensured and,
more generally, to use and operate it in the same conditions as regards security.

The fact that you are presently reading this means that you have had knowledge of the CeCILL license
and that you accept its terms.*/

#ifndef MODEL_HPP
#define MODEL_HPP

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "doctest.h"

/*

====================================================================================================
  ~*~ _Port class ~*~
==================================================================================================*/
class _VirtualPort {
  public:
    virtual ~_VirtualPort() = default;
};

template <class... Args>
class _Port : public _VirtualPort {
  public:
    std::function<void(Args...)> _set;

    _Port() = delete;
    virtual ~_Port() = default;

    template <class C>
    explicit _Port(C* ref, void (C::*prop)(Args...)) {
        _set = [=](const Args... args) { (ref->*prop)(std::forward<const Args>(args)...); };
    }
};

/*
============================================== TEST ==============================================*/
TEST_CASE("_Port tests.") {
    class MyCompo {
      public:
        int i{1};
        int j{2};
        void setIJ(int iin, int jin) {
            i = iin;
            j = jin;
        }
    };

    MyCompo compo;
    auto ptr = static_cast<_VirtualPort*>(new _Port<int, int>{&compo, &MyCompo::setIJ});
    auto ptr2 = dynamic_cast<_Port<int, int>*>(ptr);
    ptr2->_set(3, 4);
    CHECK(compo.i == 3);
    CHECK(compo.j == 4);
    delete ptr;
}

/*

====================================================================================================
  ~*~ Component class ~*~
  tinycompo components should always inherit from this class. It is mostly used as a base to be able
  to store pointers to child class instances but also provides basic debugging methods and the
  infrastructure required to declare ports.
==================================================================================================*/
class Component {
    std::map<std::string, std::unique_ptr<_VirtualPort>> ports;

  public:
    Component(const Component&) = delete;  // forbidding copy
    Component() = default;

    virtual std::string _debug() = 0;

    template <class C, class... Args>
    void port(std::string name, void (C::*prop)(Args...)) {
        ports[name] = std::unique_ptr<_VirtualPort>(
            static_cast<_VirtualPort*>(new _Port<const Args...>(dynamic_cast<C*>(this), prop)));
    }

    template <class... Args>
    void set(std::string name, Args&&... args) {
        auto ptr = dynamic_cast<_Port<const Args...>*>(ports[name].get());
        if (ptr != nullptr)  // casting succeedeed
        {
            ptr->_set(std::forward<Args>(args)...);
        } else {  // casting failed, trying to provide useful error message
            std::cout << "-- Error while trying to set property! Type "
                      << typeid(_Port<const Args...>).name() << " does not seem to match port "
                      << name << ".\n";
            exit(1);  // TODO exception?
        }
    }
};

/*
============================================== TEST ==============================================*/
class MyCompo : public Component {  // example of a user creating their own component
  public:                           // by inheriting from the Component class
    int i{1};
    int j{2};

    MyCompo(const MyCompo&) = default;  // does not work (Component's copy constructor is deleted)

    MyCompo(int i = 5, int j = 6) : i(i), j(j) {
        port("myPort", &MyCompo::setIJ);  // how to declare a port
    }

    void setIJ(int iin, int jin) {  // the setter method that acts as our port
        i = iin;
        j = jin;
    }

    std::string _debug() override { return "MyCompo"; }
};

TEST_CASE("Basic component tests.") {
    MyCompo compo{};
    // MyCompo compo2 = compo; // does not work because Component copy is forbidden (intentional)
    CHECK(compo._debug() == "MyCompo");
    compo.set("myPort", 17, 18);
    CHECK(compo.i == 17);
    CHECK(compo.j == 18);
}

/*

====================================================================================================
  ~*~ _Component class ~*~
  A small class that is capable of storing a constructor call for any Component child class and
  execute said call later on demand. The class itself is not templated (allowing direct storage)
  but the constructor call is. This is an internal tinycompo class that should never be seen by
  the user (as denoted by the underscore prefix).
==================================================================================================*/
template <class T>  // this is an empty helper class that is used to pass T to the _Component
class _Type {};     // constructor below

class _Component {
  public:
    template <class T, class... Args>
    _Component(_Type<T>, Args&&... args) {
        // stores a lambda that creates a new object of type T with provided args and returns a
        // unique pointer to the newly created object
        _constructor = [=]() {
            return std::unique_ptr<Component>(
                static_cast<Component*>(new T(std::forward<const Args>(args)...)));
        };
    }

    std::function<std::unique_ptr<Component>()> _constructor;  // stores the component constructor
};

/*
============================================== TEST ==============================================*/
TEST_CASE("_Component class tests.") {
    _Component compo(_Type<MyCompo>(), 3, 4);  // create _Component object
    auto ptr = compo._constructor();           // instantiate actual object
    auto ptr2 = dynamic_cast<MyCompo*>(ptr.get());
    CHECK(ptr2->i == 3);
    CHECK(ptr2->j == 4);
    CHECK(ptr->_debug() == "MyCompo");
}

/*

====================================================================================================
  ~*~ _Property class ~*~
==================================================================================================*/
class _Property {
  public:
    template <class... Args>
    _Property(std::string prop, Args&&... args) {
        _setter = [=](Component* compo) { compo->set(prop, std::forward<const Args>(args)...); };
    }

    std::function<void(Component*)> _setter;  // stores the component constructor
};

/*
============================================== TEST ==============================================*/
TEST_CASE("_Property class tests.") {
    _Property myProp{"myPort", 22, 23};  // note that the property is created BEFORE the instance
    MyCompo compo{11, 12};
    auto myRef = static_cast<Component*>(&compo);
    myProp._setter(myRef);
    CHECK(compo.i == 22);
    CHECK(compo.j == 23);
}

/*

====================================================================================================
  ~*~ Assembly class ~*~
==================================================================================================*/
class Assembly {
    std::map<std::string, _Component> components;
    std::map<std::string, std::unique_ptr<Component>> instances;
    std::vector<std::pair<std::string, _Property>> properties;

  public:
    template <class T, class... Args>
    void component(std::string name, Args&&... args) {
        components.emplace(std::piecewise_construct, std::forward_as_tuple(name),
                           std::forward_as_tuple(_Type<T>(), std::forward<Args>(args)...));
    }

    template <class... Args>
    void property(std::string compoName, std::string propName, Args&&... args) {
        properties.emplace_back(
            std::piecewise_construct, std::forward_as_tuple(compoName),
            std::forward_as_tuple(_Property(propName, std::forward<Args>(args)...)));
    }

    void instantiate() {
        for (auto c : components) {
            instances.emplace(c.first, c.second._constructor());
        }
        for (auto p : properties) {
            p.second._setter(instances[p.first].get());
        }
    }

    Component* get_ptr_to_instance(std::string name) { return instances[name].get(); }

    void print_all(std::ostream& os = std::cout) {
        for (auto& i : instances) {
            os << i.first << ": " << i.second->_debug() << std::endl;
        }
    }
};

/*
============================================== TEST ==============================================*/
TEST_CASE("Basic test.") {
    Assembly a;
    a.component<MyCompo>("Compo1", 13, 14);
    a.component<MyCompo>("Compo2", 15, 16);
    a.property("Compo2", "myPort", 22, 23);
    a.instantiate();
    auto ptr = dynamic_cast<MyCompo*>(a.get_ptr_to_instance("Compo1"));
    auto ptr2 = dynamic_cast<MyCompo*>(a.get_ptr_to_instance("Compo2"));
    CHECK(ptr->i == 13);
    CHECK(ptr->j == 14);
    CHECK(ptr2->i == 22);
    CHECK(ptr2->j == 23);
    std::stringstream ss;
    a.print_all(ss);
    CHECK(ss.str() == "Compo1: MyCompo\nCompo2: MyCompo\n");
}

#endif  // MODEL_HPP

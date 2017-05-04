/*
Copyright or © or Copr. Centre National de la Recherche Scientifique (CNRS) (2017/05/03)
Contributors:
- Vincent Lanore <vincent.lanore@gmail.com>

This software is a computer program whose purpose is to provide the necessary classes to write
ligntweight component-based c++ applications.

This software is governed by the CeCILL-B license under French law and abiding by the rules of
distribution of free software. You can use, modify and/ or redistribute the software under the terms
of the CeCILL-B license as circulated by CEA, CNRS and INRIA at the following URL
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

The fact that you are presently reading this means that you have had knowledge of the CeCILL-B
license and that you accept its terms.*/

#ifndef TINYCOMPO_HPP
#define TINYCOMPO_HPP

#include <cxxabi.h>
#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef EDITOR
#include "doctest.h"
#endif

/*
====================================================================================================
  ~*~ Debug ~*~
  A few classes related to debug messages.
==================================================================================================*/
#ifdef __GNUG__
std::string demangle(const char* name) {
    int status{0};

    std::unique_ptr<char, void (*)(void*)> res{abi::__cxa_demangle(name, NULL, NULL, &status),
                                               std::free};

    return (status == 0) ? res.get() : name;
}
#else
std::string demangle(const char* name) { return name; }
#endif  // DOCTEST_LIBRARY_INCLUDED

class TinycompoException : public std::exception {
  public:
    std::string message{""};
    explicit TinycompoException(const std::string& init = "") : message{init} {}
    const char* what() const noexcept override { return message.c_str(); }
};

class TinycompoDebug : public std::stringstream {
    static std::ostream* error_stream;
    std::string short_message;

  public:
    static void set_stream(std::ostream& os) { error_stream = &os; }

    explicit TinycompoDebug(const std::string& error_desc)
        : short_message(error_desc){}

              [[noreturn]] void fail() const {
        *error_stream << "-- Error: " << short_message;
        if (str() != "") {
            *error_stream << ". " << str();
        }
        throw TinycompoException(short_message);
    }
};

std::ostream* TinycompoDebug::error_stream = &std::cerr;

/*
============================================== TEST ==============================================*/
#ifdef DOCTEST_LIBRARY_INCLUDED
TEST_CASE("Exception tests") {
    std::stringstream ss{};
    std::stringstream ss2{};
    TinycompoDebug::set_stream(ss2);
    try {
        TinycompoDebug e{"my error"};
        e << "Something failed.";
        e.fail();
    } catch (TinycompoException& e) {
        ss << e.what();
    }
    CHECK(ss.str() == "my error");
    CHECK(ss2.str() == "-- Error: my error. Something failed.");
    TinycompoDebug::set_stream(std::cerr);
    CHECK(demangle("PFvPFvvEE") == "void (*)(void (*)())");
}
#endif  // DOCTEST_LIBRARY_INCLUDED

/*
====================================================================================================
  ~*~ _Port class ~*~
  A class that is initialized with a pointer to a method 'void prop(Args)' of an object of class C,
  and provides a method called '_set(Args...)' which calls prop.
  _Port<Args...> derives from _VirtualPort which allows the storage of pointers to _Port by
  converting them to _VirtualPort*. These classes are for internal use by tinycompo and should not
  be seen by the user (as denoted by the underscore prefixes).
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

    template <class C>
    explicit _Port(C* ref, void (C::*prop)(Args...))
        : _set([=](const Args... args) { (ref->*prop)(std::forward<const Args>(args)...); }) {}
};

/*
============================================== TEST ==============================================*/
#ifdef DOCTEST_LIBRARY_INCLUDED
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
    REQUIRE(ptr2 != nullptr);
    ptr2->_set(3, 4);
    CHECK(compo.i == 3);
    CHECK(compo.j == 4);
    delete ptr;
}
#endif  // DOCTEST_LIBRARY_INCLUDED

/*
====================================================================================================
  ~*~ Component class ~*~
  tinycompo components should always inherit from this class. It is mostly used as a base to be able
  to store pointers to child class instances but also provides basic debugging methods and the
  infrastructure required to declare ports.
==================================================================================================*/
class Component {
    std::map<std::string, std::unique_ptr<_VirtualPort>> _ports;

  public:
    Component(const Component&) = delete;  // forbidding copy
    Component() = default;
    virtual ~Component() = default;

    virtual std::string _debug() const = 0;

    template <class C, class... Args>
    void port(std::string name, void (C::*prop)(Args...)) {
        _ports[name] = std::unique_ptr<_VirtualPort>(
            static_cast<_VirtualPort*>(new _Port<const Args...>(dynamic_cast<C*>(this), prop)));
    }

    template <class... Args>
    void set(std::string name, Args... args) {  // no perfect forwarding to avoid references
        auto ptr = dynamic_cast<_Port<const Args...>*>(_ports[name].get());
        if (ptr != nullptr)  // casting succeedeed
        {
            ptr->_set(std::forward<Args>(args)...);
        } else {  // casting failed, trying to provide useful error message
            TinycompoDebug e{"Setting property failed"};
            e << "Type " << demangle(typeid(_Port<const Args...>).name())
              << " does not seem to match port " << name << ".\n";
            e.fail();
        }
    }
};

/*
============================================== TEST ==============================================*/
#ifdef DOCTEST_LIBRARY_INCLUDED
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

    std::string _debug() const override { return "MyCompo"; }
};

TEST_CASE("Basic component tests.") {
    MyCompo compo{};
    // MyCompo compo2 = compo; // does not work because Component copy is forbidden (intentional)
    CHECK(compo._debug() == "MyCompo");
    compo.set("myPort", 17, 18);
    CHECK(compo.i == 17);
    CHECK(compo.j == 18);

    std::stringstream my_cerr;
    std::stringstream tmp;
    try {
        TinycompoDebug::set_stream(my_cerr);
        compo.set("myPort", true);  // intentional error
    } catch (const TinycompoException& e) {
        tmp << e.what();
    }
    CHECK(tmp.str() == "Setting property failed");
    CHECK(my_cerr.str() ==
          "-- Error: Setting property failed. Type _Port<bool const> does not seem to match port "
          "myPort.\n");
    TinycompoDebug::set_stream(std::cerr);
}
#endif  // DOCTEST_LIBRARY_INCLUDED

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
        _constructor = [=]() {
            return std::unique_ptr<Component>(
                static_cast<Component*>(new T(std::forward<const Args>(args)...)));
        };
    }

    std::function<std::unique_ptr<Component>()> _constructor;  // stores the component constructor
};

/*
============================================== TEST ==============================================*/
#ifdef DOCTEST_LIBRARY_INCLUDED
TEST_CASE("_Component class tests.") {
    _Component compo(_Type<MyCompo>(), 3, 4);  // create _Component object
    auto ptr = compo._constructor();           // instantiate actual object
    auto ptr2 = dynamic_cast<MyCompo*>(ptr.get());
    REQUIRE(ptr2 != nullptr);
    CHECK(ptr2->i == 3);
    CHECK(ptr2->j == 4);
    CHECK(ptr->_debug() == "MyCompo");
}
#endif  // DOCTEST_LIBRARY_INCLUDED

/*
====================================================================================================
  ~*~ _Operation class ~*~
==================================================================================================*/
template <class A, class Key>
class _Operation {
  public:
    template <class Connector, class... Args>
    _Operation(_Type<Connector>, Args&&... args)
        : _connect([=](A& assembly) {
              Connector::_connect(assembly, std::forward<const Args>(args)...);
          }) {}

    template <class... Args>
    _Operation(Key component, const std::string& prop, Args&&... args)
        : _connect([=](A& assembly) {
              assembly.at(component).set(prop, std::forward<const Args>(args)...);
          }) {}

    std::function<void(A&)> _connect;
};

/*
============================================== TEST ==============================================*/
#ifdef DOCTEST_LIBRARY_INCLUDED
TEST_CASE("_Operation class tests.") {
    class MyAssembly {
      public:
        MyCompo compo1{14, 15};
        MyCompo compo2{18, 19};
        Component& at(int i) {
            if (i == 0) {
                return compo1;
            } else {
                return compo2;
            }
        }
    };

    class MyConnector {
      public:
        static void _connect(MyAssembly& a, int i, int i2) {
            a.compo1.i = i;
            a.compo2.i = i2;
        }
    };

    MyAssembly myAssembly;
    _Operation<MyAssembly, int> myConnection{_Type<MyConnector>(), 22, 23};
    myConnection._connect(myAssembly);
    CHECK(myAssembly.compo1.i == 22);
    CHECK(myAssembly.compo1.j == 15);
    CHECK(myAssembly.compo2.i == 23);
    CHECK(myAssembly.compo2.j == 19);
    _Operation<MyAssembly, int> myProperty{0, "myPort", 3, 4};
    myProperty._connect(myAssembly);
    CHECK(myAssembly.compo1.i == 3);
    CHECK(myAssembly.compo1.j == 4);
}
#endif  // DOCTEST_LIBRARY_INCLUDED

/*
====================================================================================================
  ~*~ Assembly class ~*~
  A class that represents a component assembly. It provides methods to declare components,
  connections, properties, to instantiate the assembly and to interact with the instantiated
  assembly. This class should be used as-is (not by inheriting from it) by users.
==================================================================================================*/
template <class Key = std::string>
class Assembly {
  protected:
    std::map<Key, _Component> components;
    std::map<Key, std::unique_ptr<Component>> instances;
    std::vector<_Operation<Assembly, Key>> operations;
    bool instantiated{false};

  public:
    template <class T, class... Args>
    void component(Key address, Args&&... args) {
        components.emplace(std::piecewise_construct, std::forward_as_tuple(address),
                           std::forward_as_tuple(_Type<T>(), std::forward<Args>(args)...));
    }

    template <class... Args>
    void property(Key compoName, std::string propName, Args&&... args) {
        operations.emplace_back(compoName, propName, std::forward<Args>(args)...);
    }

    template <class C, class... Args>
    void connect(Args&&... args) {
        operations.emplace_back(_Type<C>(), std::forward<Args>(args)...);
    }

    std::size_t size() const { return components.size(); }

    void check_instantiation(const std::string& from) const {
        if (!instantiated) {
            TinycompoDebug error{"Uninstantiated assembly."};
            error << "Trying to call method " << from
                  << " although the assembly is not instantiated!";
            error.fail();
        }
    }

    void instantiate() {
        instantiated = true;
        for (auto c : components) {
            instances.emplace(c.first, c.second._constructor());
        }
        for (auto c : operations) {
            c._connect(*this);
        }
    }

    template <class T = Component>
    T& at(Key address) const {
        check_instantiation("at (direct)");
        return dynamic_cast<T&>(*(instances.at(address).get()));
    }

    template <class T = Component, class SubKey, class... Args>
    T& at(Key address, SubKey subKey, Args... args) const {
        check_instantiation("at (sub-adressing)");
        auto& ref = at<Assembly<SubKey>>(address);
        return ref.template at<T>(subKey, std::forward<Args>(args)...);
    }

    void print_all(std::ostream& os = std::cout) const {
        check_instantiation("print_all");
        for (auto& i : instances) {
            os << i.first << ": " << i.second->_debug() << std::endl;
        }
    }

    template <class... Args>
    void call(const std::string& compo, const std::string& prop, Args... args) const {
        check_instantiation("call");
        at(compo).set(prop, std::forward<Args>(args)...);
    }
};

/*
============================================== TEST ==============================================*/
#ifdef DOCTEST_LIBRARY_INCLUDED
TEST_CASE("Basic test.") {
    class MyConnector {
      public:
        static void _connect(Assembly<>& a, int i, int i2) {
            a.at<MyCompo>("Compo1").i = i;
            a.at<MyCompo>("Compo2").i = i2;
        }
    };

    Assembly<> a;
    a.component<MyCompo>("Compo1", 13, 14);
    a.component<MyCompo>("Compo2", 15, 16);
    a.property("Compo2", "myPort", 22, 23);
    CHECK(a.size() == 2);
    a.connect<MyConnector>(33, 34);
    a.instantiate();
    auto& ref = a.at<MyCompo&>("Compo1");
    auto& ref2 = a.at<MyCompo&>("Compo2");
    CHECK(ref.i == 33);   // changed by connector
    CHECK(ref.j == 14);   // base value
    CHECK(ref2.i == 34);  // changed by property and then by connector (in declaration order)
    CHECK(ref2.j == 23);  // changed by property
    a.call("Compo2", "myPort", 77, 79);
    CHECK(ref2.i == 77);
    CHECK(ref2.j == 79);
    std::stringstream ss;
    a.print_all(ss);
    CHECK(ss.str() == "Compo1: MyCompo\nCompo2: MyCompo\n");
}

TEST_CASE("sub-addressing tests") {
    class MyComposite : public Component, public Assembly<int> {
      public:
        std::string _debug() const override { return "MyComposite"; }
    };
    Assembly<> b;
    b.component<MyComposite>("Array");
    b.instantiate();
    auto& arrayRef = b.at<MyComposite>("Array");
    arrayRef.component<MyCompo>(0, 12, 13);
    arrayRef.component<MyCompo>(1, 15, 19);
    arrayRef.component<MyComposite>(2);
    arrayRef.instantiate();
    auto& subArrayRef = arrayRef.at<MyComposite>(2);
    subArrayRef.component<MyCompo>(0, 19, 22);
    subArrayRef.component<MyCompo>(1, 7, 9);
    subArrayRef.instantiate();
    auto& subRef = b.at<MyCompo>("Array", 1);
    auto& subSubRef = b.at<MyCompo>("Array", 2, 1);
    CHECK(subRef.i == 15);
    CHECK(subSubRef.i == 7);
    std::stringstream ss;
    b.print_all(ss);
    CHECK(ss.str() == "Array: MyComposite\n");
}
#endif  // DOCTEST_LIBRARY_INCLUDED

/*
====================================================================================================
  ~*~ UseProvide class ~*~
  UseProvide is a "connector class", ie a functor that can be passed as template parameter to
  Assembly::connect. This particular connector implements the "use/provide" connection, ie setting a
  port of one component (the user) to a pointer to an interface of another (the provider). This
  class should be used as-is to declare assembly connections.
==================================================================================================*/
template <class Interface>
class UseProvide {
  public:
    static void _connect(Assembly<>& assembly, std::string user, std::string userPort,
                         std::string provider) {
        auto& refUser = assembly.at(user);
        auto& refProvider = assembly.at<Interface>(provider);
        refUser.set(userPort, &refProvider);
    }
};

/*
============================================== TEST ==============================================*/
#ifdef DOCTEST_LIBRARY_INCLUDED
class IntInterface {
  public:
    virtual int get() const = 0;
};

class MyInt : public Component, public IntInterface {
  public:
    int i{1};
    explicit MyInt(int i = 0) : i(i) {}
    std::string _debug() const override { return "MyInt"; }
    int get() const override { return i; }
};

class MyIntProxy : public Component, public IntInterface {
    IntInterface* ptr{nullptr};

  public:
    MyIntProxy() { port("ptr", &MyIntProxy::set_ptr); }
    void set_ptr(IntInterface* ptrin) { ptr = ptrin; }
    std::string _debug() const override { return "MyIntProxy"; }
    int get() const override { return 2 * ptr->get(); }
};

TEST_CASE("Use/provide test.") {
    Assembly<> model;
    model.component<MyInt>("Compo1", 4);
    model.component<MyIntProxy>("Compo2");
    model.instantiate();
    std::stringstream ss;
    model.print_all(ss);
    CHECK(ss.str() == "Compo1: MyInt\nCompo2: MyIntProxy\n");
    UseProvide<IntInterface>::_connect(model, "Compo2", "ptr", "Compo1");
    CHECK(model.at<MyIntProxy>("Compo2").get() == 8);
}
#endif  // DOCTEST_LIBRARY_INCLUDED

#include "arrays.hpp"

#endif  // TINYCOMPO_HPP

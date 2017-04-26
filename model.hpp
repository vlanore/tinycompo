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
#include "doctest.h"

/*

====================================================================================================
  ~*~ Exceptions ~*~
==================================================================================================*/
class TinycompoException : public std::exception {
  public:
    static std::ostream* error_stream;
    std::string message{""};

    explicit TinycompoException(const std::string& init = "") : message{init} {}

    const char* what() const noexcept override { return message.c_str(); }

    static void set_stream(std::ostream& os) { error_stream = &os; }

    void print() const { *error_stream << message; }
};

std::ostream* TinycompoException::error_stream = &std::cerr;

/*
============================================== TEST ==============================================*/
TEST_CASE("Exception tests") {
    std::stringstream ss{};
    std::stringstream ss2{};
    TinycompoException::set_stream(ss2);
    try {
        throw TinycompoException("ERROR");
    } catch (TinycompoException& e) {
        ss << e.what();
        e.print();
    }
    CHECK(ss.str() == "ERROR");
    CHECK(ss2.str() == "ERROR");
}

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
            std::cerr << "-- Error while trying to set property! Type "
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

    std::string _debug() const override { return "MyCompo"; }
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
    REQUIRE(ptr2 != nullptr);
    CHECK(ptr2->i == 3);
    CHECK(ptr2->j == 4);
    CHECK(ptr->_debug() == "MyCompo");
}

/*

====================================================================================================
  ~*~ _Property class ~*~
  A class that is used by Assembly to store a "property" (ie, configuring a component by calling a
  method with parameters). _Property stores a "partial application" of the call (only missing the
  assembly pointer) in a std::function. This class is not meant to be encountered by users as
  denoted by the underscore prefix.
==================================================================================================*/
class _Property {
  public:
    template <class... Args>
    _Property(const std::string& prop, Args&&... args)
        : _setter([=](Component* compo) { compo->set(prop, std::forward<const Args>(args)...); }) {}

    std::function<void(Component*)> _setter;
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
  ~*~ __Connection class ~*~
  A class that is used by Assembly to store a connection between components. A connection is an
  operation that can affect several components at once, for example to set a port of one to a
  pointer to another. This class is for internal tinycompo use and should never be used by users,
  as denoted by the underscore prefix.
==================================================================================================*/
template <class A>
class _Connection {
  public:
    template <class Connector, class... Args>
    _Connection(_Type<Connector>, Args&&... args)
        : _connect([=](A& assembly) {
              Connector::_connect(assembly, std::forward<const Args>(args)...);
          }) {}

    std::function<void(A&)> _connect;
};

/*
============================================== TEST ==============================================*/
TEST_CASE("_Connection class tests.") {
    class MyAssembly {
      public:
        MyCompo compo1{14, 15};
        MyCompo compo2{18, 19};
    };

    class MyConnector {
      public:
        static void _connect(MyAssembly& a, int i, int i2) {
            a.compo1.i = i;
            a.compo2.i = i2;
        }
    };

    MyAssembly myAssembly;
    _Connection<MyAssembly> myConnection{_Type<MyConnector>(), 22, 23};
    myConnection._connect(myAssembly);
    CHECK(myAssembly.compo1.i == 22);
    CHECK(myAssembly.compo1.j == 15);
    CHECK(myAssembly.compo2.i == 23);
    CHECK(myAssembly.compo2.j == 19);
}

/*

====================================================================================================
  ~*~ Assembly class ~*~
  A class that represents a component assembly. It provides methods to declare components,
  connections, properties, to instantiate the assembly and to interact with the instantiated
  assembly. This class should be used as-is (not by inheriting from it) by users.
==================================================================================================*/
class Assembly {
    std::map<std::string, _Component> components;
    std::map<std::string, std::unique_ptr<Component>> instances;
    std::vector<std::pair<std::string, _Property>> properties;
    std::vector<_Connection<Assembly>> connections;

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

    template <class C, class... Args>
    void connect(Args&&... args) {
        connections.emplace_back(_Type<C>(), std::forward<Args>(args)...);
    }

    void instantiate() {
        for (auto c : components) {
            instances.emplace(c.first, c.second._constructor());
        }
        for (auto c : connections) {
            c._connect(*this);
        }
        for (auto p : properties) {
            p.second._setter(instances[p.first].get());
        }
    }

    Component& get_ref_to_instance(std::string name) const { return *(instances.at(name).get()); }

    void print_all(std::ostream& os = std::cout) const {
        for (auto& i : instances) {
            os << i.first << ": " << i.second->_debug() << std::endl;
        }
    }
};

/*
============================================== TEST ==============================================*/
TEST_CASE("Basic test.") {
    class MyConnector {
      public:
        static void _connect(Assembly& a, int i, int i2) {
            dynamic_cast<MyCompo&>(a.get_ref_to_instance("Compo1")).i = i;
            dynamic_cast<MyCompo&>(a.get_ref_to_instance("Compo2")).i = i2;
        }
    };

    Assembly a;
    a.component<MyCompo>("Compo1", 13, 14);
    a.component<MyCompo>("Compo2", 15, 16);
    a.property("Compo2", "myPort", 22, 23);
    a.connect<MyConnector>(33, 34);
    a.instantiate();
    auto& ref = dynamic_cast<MyCompo&>(a.get_ref_to_instance("Compo1"));
    auto& ref2 = dynamic_cast<MyCompo&>(a.get_ref_to_instance("Compo2"));
    CHECK(ref.i == 33);   // changed by connector
    CHECK(ref.j == 14);   // base value
    CHECK(ref2.i == 22);  // changed by connector AND THEN by property
    CHECK(ref2.j == 23);  // changed by property
    std::stringstream ss;
    a.print_all(ss);
    CHECK(ss.str() == "Compo1: MyCompo\nCompo2: MyCompo\n");
}

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
    static void _connect(Assembly& assembly, std::string user, std::string userPort,
                         std::string provider) {
        auto& refUser = assembly.get_ref_to_instance(user);
        auto refProvider = dynamic_cast<Interface*>(&assembly.get_ref_to_instance(provider));
        refUser.set(userPort, refProvider);
    }
};

/*
============================================== TEST ==============================================*/
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
    Assembly model;
    model.component<MyInt>("Compo1", 4);
    model.component<MyIntProxy>("Compo2");
    model.instantiate();
    std::stringstream ss;
    model.print_all(ss);
    CHECK(ss.str() == "Compo1: MyInt\nCompo2: MyIntProxy\n");
    UseProvide<IntInterface>::_connect(model, "Compo2", "ptr", "Compo1");
    auto& ref = dynamic_cast<MyIntProxy&>(model.get_ref_to_instance("Compo2"));
    CHECK(ref.get() == 8);
}

/*

====================================================================================================
  ~*~ Array class ~*~
  Generic specialization of the Component class to represent arrays of components. All component
  arrays inherit from ComponentArray so that they can be manipulated as arrays of Component whitout
  knowing the exact class. This class should be used as a template parameter for calls to
  Assembly::component.
==================================================================================================*/
class ComponentArray {
  public:
    virtual Component& at(int index) const = 0;
    virtual std::size_t size() const = 0;
};

template <class T, std::size_t n>
class Array : public ComponentArray, public Component, public Assembly {
  public:
    std::string _debug() const override {
        std::stringstream ss;
        ss << "Array [\n";
        print_all(ss);
        ss << "]";
        return ss.str();
    }

    template <class... Args>
    explicit Array(Args... args) {
        for (int i = 0; i < static_cast<int>(n); i++) {
            std::stringstream ss;
            ss << "Element" << i;
            component<T>(ss.str(), std::forward<Args>(args)...);
        }
        instantiate();
    }

    Component& at(int index) const override {
        std::stringstream ss;
        ss << "Element" << index;
        return get_ref_to_instance(ss.str());
    }

    std::size_t size() const override { return n; }
};

/*
============================================== TEST ==============================================*/
TEST_CASE("Array tests.") {
    Array<MyCompo, 3> myArray(11, 12);
    CHECK(myArray._debug() ==
          "Array [\nElement0: MyCompo\nElement1: MyCompo\nElement2: MyCompo\n]");
    CHECK(myArray.size() == 3);
    auto& ref0 = dynamic_cast<MyCompo&>(myArray.at(0));
    auto& ref2 = dynamic_cast<MyCompo&>(myArray.at(2));
    ref2.i = 17;          // random number
    CHECK(ref0.i == 11);  // cf myArray init above
    CHECK(ref2.i == 17);
    auto& ref2bis = dynamic_cast<MyCompo&>(myArray.at(2));
    CHECK(ref2bis.i == 17);
}

/*

====================================================================================================
  ~*~ ArrayOneToOne class ~*~
  This is a connector that takes two arrays with identical sizes and connects (as if using the
  UseProvide connector) every i-th element in array1 to its corresponding element in array2 (ie,
  the i-th element in array2). This class should be used as a template parameter for
  Assembly::connect.
==================================================================================================*/
template <class Interface>
class ArrayOneToOne {
  public:
    static void _connect(Assembly& a, std::string array1, std::string prop, std::string array2) {
        auto& ref1 = dynamic_cast<ComponentArray&>(a.get_ref_to_instance(array1));
        auto& ref2 = dynamic_cast<ComponentArray&>(a.get_ref_to_instance(array2));
        if (ref1.size() == ref2.size()) {
            for (int i = 0; i < static_cast<int>(ref1.size()); i++) {
                auto ptr = dynamic_cast<Interface*>(&ref2.at(i));
                ref1.at(i).set(prop, ptr);
            }
        } else {
            std::cerr << "-- Error while connecting arrays! Arrays have different sizes. " << array1
                      << " has size " << array1.size() << " while " << array2 << " has size "
                      << array2.size() << ".\n";
            exit(1);
        }
    }
};

/*
============================================== TEST ==============================================*/
TEST_CASE("Array connector tests.") {
    Assembly model;
    model.component<Array<MyInt, 5>>("intArray", 12);
    model.component<Array<MyIntProxy, 5>>("proxyArray");
    model.instantiate();
    ArrayOneToOne<IntInterface>::_connect(model, "proxyArray", "ptr", "intArray");
    auto& refArray1 = dynamic_cast<ComponentArray&>(model.get_ref_to_instance("intArray"));
    auto& refElement1 = dynamic_cast<MyInt&>(refArray1.at(1));
    CHECK(refElement1.get() == 12);
    refElement1.i = 23;
    CHECK(refElement1.get() == 23);
    auto& refArray2 = dynamic_cast<ComponentArray&>(model.get_ref_to_instance("proxyArray"));
    auto& refElement2 = dynamic_cast<MyIntProxy&>(refArray2.at(1));
    CHECK(refElement2.get() == 46);
    auto& refElement3 = dynamic_cast<MyIntProxy&>(refArray2.at(4));
    CHECK(refElement3.get() == 24);
}

/*

====================================================================================================
  ~*~ Reduce class ~*~
  The Reduce class is a connector that connects (as if using the UseProvide connector) one port of
  one component (the reducer) to every component in an array. This can be seen as a "multiple use"
  connector (the reducer is the user in multiple use/provide connections). This class should be
  used as a template parameter for Assembly::connect.
==================================================================================================*/
template <class Interface>
class Reduce {
  public:
    static void _connect(Assembly& a, std::string reducer, std::string prop, std::string array) {
        auto& ref1 = dynamic_cast<Component&>(a.get_ref_to_instance(reducer));
        auto& ref2 = dynamic_cast<ComponentArray&>(a.get_ref_to_instance(array));
        for (int i = 0; i < static_cast<int>(ref2.size()); i++) {
            auto ptr = dynamic_cast<Interface*>(&ref2.at(i));
            ref1.set(prop, ptr);
        }
    }
};

/*
============================================== TEST ==============================================*/
class IntReducer : public Component, public IntInterface {
    std::vector<IntInterface*> ptrs;

  public:
    std::string _debug() const override { return "IntReducer"; }

    void addPtr(IntInterface* ptr) { ptrs.push_back(ptr); }

    int get() const override {
        int i = 0;
        for (auto ptr : ptrs) {
            i += ptr->get();
        }
        return i;
    }

    IntReducer() { port("ptrs", &IntReducer::addPtr); }
};

TEST_CASE("Array connector tests.") {
    Assembly model;
    model.component<Array<MyInt, 3>>("intArray", 12);
    model.component<IntReducer>("Reducer");
    model.instantiate();
    std::stringstream ss;
    model.print_all(ss);
    CHECK(ss.str() ==
          "Reducer: IntReducer\nintArray: Array [\nElement0: MyInt\nElement1: MyInt\nElement2: "
          "MyInt\n]\n");
    Reduce<IntInterface>::_connect(model, "Reducer", "ptrs", "intArray");
    auto& refArray = dynamic_cast<ComponentArray&>(model.get_ref_to_instance("intArray"));
    auto& refElement1 = dynamic_cast<MyInt&>(refArray.at(1));
    CHECK(refElement1.get() == 12);
    refElement1.i = 23;
    CHECK(refElement1.get() == 23);
    auto& refReducer = dynamic_cast<IntInterface&>(model.get_ref_to_instance("Reducer"));
    CHECK(refReducer.get() == 47);
}

/*

====================================================================================================
                                      ~*~*~ FULL TEST ~*~*~
==================================================================================================*/
TEST_CASE("Large test resembling a real-life situation more than the other tests") {
    /*  The model :
        -----------                      ------------------
        |  MyInt  |<== UseProvide ==(ptr)|   MyIntProxy   |
        | value:4 |                      | val:2*intGet() |
        -----------                      ------------------
     */
    Assembly model;
    model.component<MyInt>("Compo1", 4);
    model.component<MyIntProxy>("Compo2");
    model.connect<UseProvide<IntInterface>>("Compo2", "ptr", "Compo1");
    model.instantiate();
    std::stringstream ss;
    model.print_all(ss);
    CHECK(ss.str() == "Compo1: MyInt\nCompo2: MyIntProxy\n");
    auto& ptr = dynamic_cast<MyIntProxy&>(model.get_ref_to_instance("Compo2"));
    CHECK(ptr.get() == 8);
}

#endif  // MODEL_HPP

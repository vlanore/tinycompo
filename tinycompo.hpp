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
requirements in conditions enabling the security of their systems and/or globalModel to be ensured
and,
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
#endif

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

    [[noreturn]] void fail() const {
        *error_stream << "-- Error: " << short_message;
        if (str() != "") {
            *error_stream << ". " << str();
        }
        throw TinycompoException(short_message);
    }

    explicit TinycompoDebug(const std::string& error_desc) : short_message(error_desc) {}
};

std::ostream* TinycompoDebug::error_stream = &std::cerr;

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
====================================================================================================
  ~*~ Model ~*~
==================================================================================================*/
template <class Key>
class Assembly;  // forward-decl

template <class Key = std::string>
class Model {
  public:
    std::map<Key, _Component> components;
    std::vector<_Operation<Assembly<Key>, Key>> operations;
    void merge(const Model& newData) {
        components.insert(newData.components.begin(), newData.components.end());
        operations.insert(operations.end(), newData.operations.begin(), newData.operations.end());
    }

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
};

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
    std::map<Key, std::unique_ptr<Component>> instances;
    Model<Key> model;
    bool instantiated{false};

  public:
    Assembly() = delete;
    explicit Assembly(const Model<Key>& model) : model(model) { instantiate(); }

    std::size_t size() const { return model.size(); }

    // void check_instantiation(const std::string& from) const {
    //     if (!instantiated) {
    //         TinycompoDebug error{"uninstantiated assembly"};
    //         error << "Trying to call method " << from
    //               << " although the assembly is not instantiated!";
    //         error.fail();
    //     }
    // }

    void instantiate() {
        instantiated = true;
        for (auto c : model.components) {
            instances.emplace(c.first, c.second._constructor());
        }
        for (auto c : model.operations) {
            c._connect(*this);
        }
    }

    template <class T = Component>
    T& at(Key address) const {
        // check_instantiation("at (direct)");
        return dynamic_cast<T&>(*(instances.at(address).get()));
    }

    template <class T = Component, class SubKey, class... Args>
    T& at(Key address, SubKey subKey, Args... args) const {
        // check_instantiation("at (sub-adressing)");
        auto& ref = at<Assembly<SubKey>>(address);
        return ref.template at<T>(subKey, std::forward<Args>(args)...);
    }

    void print_all(std::ostream& os = std::cout) const {
        // check_instantiation("print_all");
        for (auto& i : instances) {
            os << i.first << ": " << i.second->_debug() << std::endl;
        }
    }

    template <class... Args>
    void call(const std::string& compo, const std::string& prop, Args... args) const {
        // check_instantiation("call");
        at(compo).set(prop, std::forward<Args>(args)...);
    }
};

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
====================================================================================================
  ~*~ Array class ~*~
  Generic specialization of the Component class to represent arrays of components. All component
  arrays inherit from Assembly<int> so that they can be manipulated as arrays of Component whitout
  knowing the exact class. This class should be used as a template parameter for calls to
  Assembly::component.
==================================================================================================*/
template <class T>
class Array : public Assembly<int>, public Component {
  public:
    std::string _debug() const override {
        std::stringstream ss;
        ss << "Array [\n";
        print_all(ss);
        ss << "]";
        return ss.str();
    }

    template <class... Args>
    explicit Array(int nbElems, Args... args) : Assembly<int>(Model<int>()) {
        for (int i = 0; i < nbElems; i++) {
            model.component<T>(i, std::forward<Args>(args)...);
        }
        instantiate();
    }
};

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
    static void _connect(Assembly<>& a, std::string array1, std::string prop, std::string array2) {
        auto& ref1 = a.at<Assembly<int>>(array1);
        auto& ref2 = a.at<Assembly<int>>(array2);
        if (ref1.size() == ref2.size()) {
            for (int i = 0; i < static_cast<int>(ref1.size()); i++) {
                auto ptr = dynamic_cast<Interface*>(&ref2.at(i));
                ref1.at(i).set(prop, ptr);
            }
        } else {
            TinycompoDebug e{"Array connection: mismatched sizes"};
            e << array1 << " has size " << ref1.size() << " while " << array2 << " has size "
              << ref2.size() << ".\n";
            e.fail();
        }
    }
};

/*
====================================================================================================
  ~*~ MultiUse class ~*~
  The MultiUse class is a connector that connects (as if using the UseProvide connector) one port of
  one component to every component in an array. This can be seen as a "multiple use" connector (the
  reducer is the user in multiple use/provide connections). This class should be used as a template
  parameter for Assembly::connect.
==================================================================================================*/
template <class Interface>
class MultiUse {
  public:
    static void _connect(Assembly<>& a, std::string reducer, std::string prop, std::string array) {
        auto& ref1 = a.at<Component>(reducer);
        auto& ref2 = a.at<Assembly<int>>(array);
        for (int i = 0; i < static_cast<int>(ref2.size()); i++) {
            auto ptr = dynamic_cast<Interface*>(&ref2.at(i));
            ref1.set(prop, ptr);
        }
    }
};

/*
====================================================================================================
  ~*~ MultiProvide class ~*~
==================================================================================================*/
template <class Interface>
class MultiProvide {
  public:
    static void _connect(Assembly<>& a, std::string array, std::string prop, std::string mapper) {
        for (int i = 0; i < static_cast<int>(a.at<Assembly<int>>(array).size()); i++) {
            a.at(array, i).set(prop, &a.at<Interface>(mapper));
        }
    }
};

/*
====================================================================================================
  ~*~ Tree ~*~
  A special composite whose internal components form a tree.
==================================================================================================*/
using TreeRef = int;

class Tree : public Assembly<TreeRef>, public Component {
    std::vector<TreeRef> parent;
    std::vector<std::vector<TreeRef>> children;

  public:
    explicit Tree(const Model<TreeRef>& model = Model<TreeRef>()) : Assembly<TreeRef>(model) {}

    std::string _debug() const override { return "Tree"; }

    template <class T, class... Args>
    TreeRef addRoot(Args&&... args) {
        if (size() != 0) {
            TinycompoDebug("trying to add root to non-empty Tree.").fail();
        } else {
            model.component<T>(0, std::forward<Args>(args)...);
            children.emplace_back();  // empty children list for root
            parent.push_back(-1);     // root has no parents :'(
            return 0;
        }
    }

    template <class T, class... Args>
    TreeRef addChild(TreeRef refParent, Args&&... args) {
        auto nodeRef = parent.size();
        model.component<T>(nodeRef, std::forward<Args>(args)...);
        parent.push_back(refParent);
        children.emplace_back();  // empty children list for newly added node
        children.at(refParent).push_back(nodeRef);
        return nodeRef;
    }

    TreeRef getParent(TreeRef refChild) { return parent.at(refChild); }

    const std::vector<TreeRef>& getChildren(TreeRef refParent) { return children.at(refParent); }
};

/*
====================================================================================================
  ~*~ ToChildren ~*~
  A tree connector that connects every node to its children.
==================================================================================================*/
template <class Interface, class Key = std::string>
class ToChildren {
  public:
    static void _connect(Assembly<>& a, Key tree, std::string prop) {
        auto& treeRef = a.at<Tree>(tree);
        for (auto i = 0; i < static_cast<int>(treeRef.size()); i++) {  // for each node...
            auto& nodeRef = treeRef.at(i);
            auto& children = treeRef.getChildren(i);
            for (auto j : children) {  // for every one of its children...
                nodeRef.set(prop, &treeRef.template at<Interface>(j));
            }
        }
    }
};

#endif  // TINYCOMPO_HPP

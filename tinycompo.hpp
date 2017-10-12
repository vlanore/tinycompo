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
#include <string.h>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
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
    std::unique_ptr<char, void (*)(void*)> res{abi::__cxa_demangle(name, NULL, NULL, &status), std::free};
    return (status == 0) ? res.get() : name;
}
#else
std::string demangle(const char* name) { return name; }
#endif

struct TinycompoException : public std::exception {
    std::string message{""};
    explicit TinycompoException(const std::string& init = "") : message{init} {}
    const char* what() const noexcept override { return message.c_str(); }
};

class TinycompoDebug : public std::stringstream {
    static std::ostream* error_stream;
    std::string short_message;

  public:
    static void set_stream(std::ostream& os) { error_stream = &os; }

    template <class T>
    static std::string type() {
        return demangle(typeid(T).name());
    }

    [[noreturn]] void fail() const {
        *error_stream << "-- Error: " << short_message;
        if (str() != "") {
            *error_stream << ". " << str();
        }
        *error_stream << std::endl;
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
struct _Port : public _VirtualPort {
    std::function<void(Args...)> _set;

    _Port() = delete;

    template <class C>
    explicit _Port(C* ref, void (C::*prop)(Args...))
        : _set([=](const Args... args) { (ref->*prop)(std::forward<const Args>(args)...); }) {}

    template <class C, class Type>
    explicit _Port(C* ref, Type(C::*prop)) : _set([=](const Type arg) { ref->*prop = arg; }) {}
};

template <class Interface>
struct _ProvidePort : public _VirtualPort {
    std::function<Interface*()> _get;

    _ProvidePort() = delete;

    template <class C>
    explicit _ProvidePort(C* ref, Interface* (C::*prop)()) : _get([=]() { return (ref->*prop)(); }) {}
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
    std::string name{""};

  public:
    Component(const Component&) = delete;  // forbidding copy
    Component() = default;
    virtual ~Component() = default;

    virtual std::string _debug() const { return "Component"; };

    template <class C, class... Args>
    void port(std::string name, void (C::*prop)(Args...)) {
        _ports[name] = std::unique_ptr<_VirtualPort>(
            static_cast<_VirtualPort*>(new _Port<const Args...>(dynamic_cast<C*>(this), prop)));
    }

    template <class C, class Arg>
    void port(std::string name, Arg(C::*prop)) {
        _ports[name] =
            std::unique_ptr<_VirtualPort>(static_cast<_VirtualPort*>(new _Port<const Arg>(dynamic_cast<C*>(this), prop)));
    }

    template <class C, class Interface>
    void provide(std::string name, Interface* (C::*prop)()) {
        _ports[name] = std::unique_ptr<_VirtualPort>(
            static_cast<_VirtualPort*>(new _ProvidePort<Interface>(dynamic_cast<C*>(this), prop)));
    }

    template <class... Args>
    void set(std::string name, Args... args) {    // no perfect forwarding to avoid references
        if (_ports.find(name) == _ports.end()) {  // there exists no port with this name
            TinycompoDebug e{"port name not found"};
            e << "Could not find port " << name << " in component " << _debug() << ".";
            e.fail();
        } else {  // there exists a port with this name
            auto ptr = dynamic_cast<_Port<const Args...>*>(_ports[name].get());
            if (ptr != nullptr)  // casting succeedeed
            {
                ptr->_set(std::forward<Args>(args)...);
            } else {  // casting failed, trying to provide useful error message
                TinycompoDebug e{"setting property failed"};
                e << "Type " << demangle(typeid(_Port<const Args...>).name()) << " does not seem to match port " << name
                  << '.';
                e.fail();
            }
        }
    }

    template <class Interface>
    Interface* get(std::string name) {
        return dynamic_cast<_ProvidePort<Interface>*>(_ports[name].get())->_get();
    }

    void set_name(const std::string& n) { name = n; }
    std::string get_name() { return name; }
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

struct _Component {
    template <class T, class... Args>
    _Component(_Type<T>, Args... args)
        : _constructor([=]() { return std::unique_ptr<Component>(dynamic_cast<Component*>(new T(args...))); }),
          _class_name(TinycompoDebug::type<T>()) {}

    std::function<std::unique_ptr<Component>()> _constructor;  // stores the component constructor
    std::string _class_name{""};
};

/*
====================================================================================================
  ~*~ Address ~*~
==================================================================================================*/
template <class Type>
class _Key {
    Type value;

  public:
    using actualType = Type;
    explicit _Key(Type value) : value(value) {}
    Type get() const { return value; }
    void set(Type new_value) { value = new_value; }
    std::string to_string() { return std::to_string(value); }
};

template <>
class _Key<const char*> {
    std::string value;

  public:
    using actualType = std::string;
    explicit _Key(const char* value) : value(value) {}
    std::string get() const { return value; }
    void set(std::string new_value) { value = new_value; }
    std::string to_string() { return get(); }
};

template <>
class _Key<std::string> {
    std::string value;

  public:
    using actualType = std::string;
    explicit _Key(const std::string& value) : value(value) {}
    std::string get() const { return value; }
    void set(std::string new_value) { value = new_value; }
    std::string to_string() { return get(); }
};

class _AbstractAddress {};

template <class Key, class... Keys>
struct _Address : public _AbstractAddress {
    std::string to_string() const {
        std::stringstream ss;
        ss << key.get() << "_" << rest.to_string();
        return ss.str();
    }

    const _Key<Key> key;
    const bool final{false};
    const _Address<Keys...> rest;

    _Address(Key key, Keys... keys) : key(key), rest(keys...) {}
};

template <class Key>
struct _Address<Key> : public _AbstractAddress {
    std::string to_string() const {
        std::stringstream ss;
        ss << key.get();
        return ss.str();
    }

    const _Key<Key> key;
    const bool final{true};

    explicit _Address(Key key) : key(key) {}
};

template <class... Keys>
_Address<Keys...> Address(Keys... keys) {
    return _Address<Keys...>(keys...);
}

template <class... Keys>
struct _PortAddress {
    std::string prop;
    _Address<Keys...> address;

    _PortAddress(const std::string& prop, Keys... keys) : prop(prop), address(Address(std::forward<Keys>(keys)...)) {}
};

template <class... Keys>
_PortAddress<Keys...> PortAddress(std::string prop, Keys... keys) {
    return _PortAddress<Keys...>(prop, std::forward<Keys>(keys)...);
}

/*
====================================================================================================
  ~*~ _Operation class ~*~
==================================================================================================*/
template <class A, class Key>
class _Operation {
    template <class... Keys>
    std::string get_args(const std::string& s, const std::string& prefix, _Address<Keys...> a) {
        std::stringstream ss;
        ss << s << " -- " << prefix << a.to_string() << " ;\n";
        return ss.str();
    }

    template <class... Keys>
    std::string get_args(const std::string& s, const std::string& prefix, _PortAddress<Keys...> a) {
        std::stringstream ss;
        ss << s << " -- " << prefix << a.address.to_string() << " [label=\"" << a.prop << "\"];\n";
        return ss.str();
    }

    template <class Arg>
    std::string get_args(const std::string&, const std::string&, Arg) {
        return "";
    }

    template <class Arg, class... Args>
    std::string get_args(const std::string& s, const std::string& prefix, Arg arg, Args... args) {
        std::stringstream ss;
        ss << get_args(s, prefix, arg) << get_args(s, prefix, std::forward<Args>(args)...);
        return ss.str();
    }

  public:
    template <class Connector, class... Args>
    _Operation(_Type<Connector>, Args... args)
        : _connect([=](A& assembly) { Connector::_connect(assembly, args...); }),
          _debug([=](std::string s, std::string prefix) {
              return s + "[xlabel=\"" + TinycompoDebug::type<Connector>() + "\" shape=point];\n" +
                     get_args(s, prefix, args...);
          }) {}

    std::function<void(A&)> _connect;
    std::function<std::string(std::string, std::string)> _debug{[](std::string, std::string) { return ""; }};
};

/*
====================================================================================================
  ~*~ Random utility classes and declarations ~*~
==================================================================================================*/
template <class Key>
class Assembly;  // forward-decl
template <class Key>
class Model;  // forward-decl

/*
====================================================================================================
  ~*~ Composite ~*~
==================================================================================================*/
struct _AbstractComposite {  // inheritance-only class
    virtual ~_AbstractComposite() = default;
};

template <class Key = std::string>
class Composite : public Model<Key>, public _AbstractComposite {};

struct _DotData {
    std::string output;
    std::vector<std::string> composite_names;
};

class _Composite {
    std::unique_ptr<_AbstractComposite> ptr;
    std::function<_AbstractComposite*()> _clone;

  public:
    std::function<Component*(std::string)> _constructor;
    std::function<_DotData(std::string)> _debug;

    _Composite() = delete;

    template <class T, class... Args>
    explicit _Composite(_Type<T>, Args&&... args)
        : ptr(std::unique_ptr<_AbstractComposite>(new T(std::forward<Args>(args)...))),
          _clone([=]() { return static_cast<_AbstractComposite*>(new T(dynamic_cast<T&>(*ptr.get()))); }),
          _constructor([=](std::string s) {
              return static_cast<Component*>(new Assembly<typename T::KeyType>(dynamic_cast<T&>(*ptr.get()), s));
          }),
          _debug([=](std::string s) { return dynamic_cast<T&>(*ptr.get())._debug(s); }) {}

    _Composite(const _Composite& other) : ptr(other._clone()), _clone(other._clone), _constructor(other._constructor) {}

    _AbstractComposite* get() { return ptr.get(); }
};

/*
====================================================================================================
  ~*~ Graph representation classes ~*~
  Small classes implementing a simple easily explorable graph representation for TinyCompo component
  assemblies.
==================================================================================================*/
class _GraphAddress {
    std::string address;
    std::string port;

  public:
    _GraphAddress(const std::string& address, const std::string& port = "") : address(address), port(port) {}

    void print() { std::cout << "->" << address << ((port == "") ? "" : ("." + port)); }
};

template <class Key>
struct _Node {
    std::string name;
    std::string type;
    std::vector<_GraphAddress> neighbors;

    void neighbors_from_args() {}

    template <class Arg, class... Args>
    void neighbors_from_args(Arg, Args... args) {
        neighbors_from_args(args...);
    }

    template <class... Keys, class... Args>
    void neighbors_from_args(_Address<Keys...> arg, Args... args) {
        neighbors.push_back(_GraphAddress(arg.to_string()));
        neighbors_from_args(args...);
    }

    template <class... Keys, class... Args>
    void neighbors_from_args(_PortAddress<Keys...> arg, Args... args) {
        neighbors.push_back(_GraphAddress(arg.address.to_string(), arg.prop));
        neighbors_from_args(args...);
    }

    void print(int tabs = 0) {
        std::cout << std::string(tabs, '\t') << ((name == "") ? "Connector" : name) << " (" << type << ") ";
        for (auto& n : neighbors) {
            n.print();
            std::cout << " ";
        }
        std::cout << '\n';
    }
};

template <class Key>
struct _AssemblyGraph {
    std::vector<_Node<Key>> components;
    std::vector<_Node<Key>> connectors;

    void print(int tabs = 0) {
        for (auto& c : components) {
            c.print(tabs);
        }
        for (auto& c : connectors) {
            c.print(tabs);
        }
    }
};

/*
====================================================================================================
  ~*~ Model ~*~
==================================================================================================*/
struct _AbstractModel {
    virtual void print_representation(int) = 0;
};

template <class Key = std::string>
class Model : public _AbstractModel {
    template <class T, bool b>
    struct _Helper {
        template <class... Args>
        static void declare(Model<Key>& model, Args&&... args) {
            model.component<T>(std::forward<Args>(args)...);
        }
    };

    template <class T>
    struct _Helper<T, true> {
        template <class... Args>
        static void declare(Model<Key>& model, Args&&... args) {
            model.composite<T>(std::forward<Args>(args)...);
        }
    };

    std::string replace_string(std::string subject, const std::string& search, const std::string& replace) {
        size_t pos = 0;
        while ((pos = subject.find(search, pos)) != std::string::npos) {
            subject.replace(pos, search.length(), replace);
            pos += replace.length();
        }
        return subject;
    }

    template <bool is_composite, class T, class Key1, class... Args>
    void _route(_Address<Key1> address, Args&&... args) {
        _Helper<T, is_composite>::declare(*this, address.key.get(), std::forward<Args>(args)...);
    }

    template <bool is_composite, class T, class Key1, class Key2, class... Keys, class... Args>
    void _route(_Address<Key1, Key2, Keys...> address, Args&&... args) {
        auto compositeIt = composites.find(address.key.get());
        if (compositeIt != composites.end()) {
            auto ptr = dynamic_cast<Model<typename _Key<Key2>::actualType>*>(compositeIt->second.get());
            if (ptr == nullptr) {
                TinycompoDebug e("key type does not match composite key type");
                e << "Key has type " << TinycompoDebug::type<typename _Key<Key2>::actualType>() << " while composite "
                  << address.key.get() << " seems to have another key type.";
                e.fail();
            }
            ptr->template _route<is_composite, T>(address.rest, std::forward<Args>(args)...);
        } else {
            TinycompoDebug e("composite does not exist");
            e << "Assembly contains no composite at address " << address.key.get() << '.';
            e.fail();
        }
    }

    _DotData _debug(const std::string& myname = "") {
        std::string prefix = myname == "" ? "" : myname + "_";
        std::stringstream ss;
        ss << (myname == "" ? "graph g {\n" : "subgraph cluster_" + myname + " {\n");

        for (auto& c : components) {
            ss << prefix << c.first << "[label=\"" << c.first << "\\n(" << c.second._class_name
               << ")\" shape=component margin=0.15];\n";
        }
        auto i = 0;
        for (auto& o : operations) {
            std::stringstream port_name;
            port_name << prefix << i;
            ss << o._debug(port_name.str(), prefix);
            i++;
        }
        _DotData data;
        for (auto& c : composites) {
            std::stringstream composite_name;
            composite_name << prefix << c.first;
            _DotData data_bis = c.second._debug(composite_name.str());
            ss << data_bis.output;
            data.composite_names.insert(data.composite_names.end(), data_bis.composite_names.begin(),
                                        data_bis.composite_names.end());
            data.composite_names.push_back(composite_name.str());
        }
        ss << "}\n";
        data.output = ss.str();
        for (auto& name : data.composite_names) {
            data.output = replace_string(data.output, "-- " + name + " ", "-- cluster_" + name + " ");
        }
        return data;
    }

    template <class>
    friend class Assembly;  // to access internal data

    template <class>
    friend class Model;  // to call _route

    friend class _Composite;  // to call _debug

  protected:
    std::map<Key, _Component> components;
    std::vector<_Operation<Assembly<Key>, Key>> operations;
    std::map<Key, _Composite> composites;

    _AssemblyGraph<Key> representation;

  public:
    using KeyType = Key;

    Model() = default;

    void merge(const Model& new_data) {
        components.insert(new_data.components.begin(), new_data.components.end());
        operations.insert(operations.end(), new_data.operations.begin(), new_data.operations.end());
        composites.insert(composites.end(), new_data.composites.begin(), new_data.composites.end());
    }

    // horrible enable_if to avoid ambiguous call with version below
    template <class T, class CallKey, class... Args,
              typename std::enable_if<!std::is_base_of<_AbstractAddress, CallKey>::value, int>::type = 0>
    void component(CallKey address, Args&&... args) {
        if (!std::is_base_of<Component, T>::value) {
            TinycompoDebug("trying to declare a component that does not inherit from Component").fail();
        }
        _Key<CallKey> key(address);
        components.emplace(std::piecewise_construct, std::forward_as_tuple(key.get()),
                           std::forward_as_tuple(_Type<T>(), std::forward<Args>(args)...));

        // FIXME temporary
        representation.components.push_back(_Node<Key>());
        representation.components.back().name = key.to_string();
        representation.components.back().type = TinycompoDebug::type<T>();
    }

    template <class T, class Key1, class Key2, class... Keys, class... Args>
    void component(_Address<Key1, Key2, Keys...> address, Args&&... args) {
        _route<false, T>(address, std::forward<Args>(args)...);
    }

    template <class T, class... Args>
    void composite(Key key, Args&&... args) {
        composites.emplace(std::piecewise_construct, std::forward_as_tuple(key),
                           std::forward_as_tuple(_Type<T>(), std::forward<Args>(args)...));
    }

    template <class T, class... Keys, class... Args>
    void composite(_Address<Keys...> address, Args&&... args) {
        _route<true, T>(address, args...);
    }

    template <class CompositeType>
    CompositeType& get_composite(const Key& address) {
        auto compositeIt = composites.find(address);
        return dynamic_cast<CompositeType&>(*compositeIt->second.get());
    }

    template <class C, class... Args>
    void connect(Args&&... args) {
        operations.emplace_back(_Type<C>(), args...);

        // FIXME temporary
        representation.connectors.push_back(_Node<Key>());
        representation.connectors.back().type = TinycompoDebug::type<C>();
        representation.connectors.back().neighbors_from_args(args...);
    }

    std::size_t size() const { return components.size() + composites.size(); }

    void dot(std::ostream& stream = std::cout) { stream << _debug().output; }

    void dot_to_file(const std::string& fileName = "tmp.dot") {
        std::ofstream file;
        file.open(fileName);
        dot(file);
        file.close();
    }

    void print_representation(int tabs = 0) {
        representation.print(tabs);
        for (auto& c : composites) {
            std::cout << std::string(tabs, '\t') << "Composite " << c.first << " {\n";
            dynamic_cast<_AbstractModel*>(c.second.get())->print_representation(tabs + 1);
            std::cout << std::string(tabs, '\t') << "}\n";
        }
    }
};

/*
====================================================================================================
  ~*~ Assembly class ~*~
==================================================================================================*/
template <class Key = std::string>
class Assembly : public Component {
  protected:
    std::map<Key, std::unique_ptr<Component>> instances;
    Model<Key>& internal_model;

  public:
    Assembly() = delete;
    explicit Assembly(Model<Key>& model, const std::string& name = "") : internal_model(model) {
        set_name(name);
        for (auto& c : model.components) {
            instances.emplace(c.first, std::unique_ptr<Component>(c.second._constructor()));
            std::stringstream ss;
            ss << get_name() << ((get_name() != "") ? "_" : "") << c.first;
            instances.at(c.first).get()->set_name(ss.str());
        }
        for (auto& c : model.composites) {
            std::stringstream ss;
            ss << get_name() << ((get_name() != "") ? "_" : "") << c.first;
            instances.emplace(c.first, std::unique_ptr<Component>(c.second._constructor(ss.str())));
        }
        for (auto& o : model.operations) {
            o._connect(*this);
        }
    }

    std::string _debug() const override {
        std::stringstream ss;
        ss << "Composite {\n";
        print_all(ss);
        ss << "}";
        return ss.str();
    }

    std::size_t size() const { return instances.size(); }

    template <class T = Component>
    T& at(Key address) const {
        try {
            return dynamic_cast<T&>(*(instances.at(address).get()));
        } catch (std::out_of_range) {
            TinycompoDebug e{"<Assembly::at> Trying to access incorrect address"};
            e << "Address " << address << " does not exist. Existing addresses are:\n";
            for (auto& key : instances) {
                e << "  * " << key.first << "\n";
            }
            e.fail();
        }
    }

    template <class T = Component, class Key1>
    T& at(const _Address<Key1>& address) const {
        return at<T>(address.key.get());
    }

    template <class T = Component, class Key1, class Key2, class... Keys>
    T& at(const _Address<Key1, Key2, Keys...>& address) const {
        return at<Assembly<typename _Key<Key2>::actualType>>(address.key.get()).template at<T>(address.rest);
    }

    Model<Key>& model() const { return internal_model; }

    void print_all(std::ostream& os = std::cout) const {
        for (auto& i : instances) {
            os << i.first << ": " << i.second->_debug() << std::endl;
        }
    }

    std::set<Key> all_keys() const {
        std::set<Key> result;
        for (auto& c : instances) {
            result.insert(c.first);
        }
        return result;
    }

    template <class... Args>
    void call(const std::string& compo, const std::string& prop, Args... args) const {
        at(compo).set(prop, std::forward<Args>(args)...);
    }
};

/*
====================================================================================================
  ~*~ Set class ~*~
==================================================================================================*/
struct Set {
    template <class Key, class... Keys, class... Args>
    static void _connect(Assembly<Key>& assembly, _PortAddress<Keys...> component, Args... args) {
        assembly.at(component.address).set(component.prop, std::forward<Args>(args)...);
    }
};

/*
====================================================================================================
  ~*~ Use class ~*~
  UseProvide is a "connector class", ie a functor that can be passed as template parameter to
  Assembly::connect. This particular connector implements the "use/provide" connection, ie setting a
  port of one component (the user) to a pointer to an interface of another (the provider). This
  class should be used as-is to declare assembly connections.
==================================================================================================*/
template <class Interface>
struct Use {
    template <class Key, class... Keys, class... Keys2>
    static void _connect(Assembly<Key>& assembly, _PortAddress<Keys...> user, _Address<Keys2...> provider) {
        auto& ref_user = assembly.at(user.address);
        auto& ref_provider = assembly.template at<Interface>(provider);
        ref_user.set(user.prop, &ref_provider);
    }
};

/*
====================================================================================================
  ~*~ ListUse class ~*~
==================================================================================================*/
template <class Interface>
struct ListUse {
    template <class Key, class UserAddress, class ProviderAddress>
    static void _connect(Assembly<Key>& assembly, UserAddress user, ProviderAddress provider) {
        Use<Interface>::_connect(assembly, user, provider);
    }

    template <class Key, class UserAddress, class ProviderAddress, class... OtherProviderAddresses>
    static void _connect(Assembly<Key>& assembly, UserAddress user, ProviderAddress provider,
                         OtherProviderAddresses... other_providers) {
        _connect(assembly, user, provider);
        _connect(assembly, user, std::forward<OtherProviderAddresses>(other_providers)...);
    }
};

/*
====================================================================================================
  ~*~ UseProvide class ~*~
==================================================================================================*/
template <class Interface>
struct UseProvide {
    template <class Key, class... Keys, class... Keys2>
    static void _connect(Assembly<Key>& assembly, _PortAddress<Keys...> user, _PortAddress<Keys2...> provider) {
        auto& ref_user = assembly.at(user.address);
        auto& ref_provider = assembly.at(provider.address);
        ref_user.set(user.prop, ref_provider.template get<Interface>(provider.prop));
    }
};

/*
====================================================================================================
  ~*~ Array class ~*~
==================================================================================================*/
template <class T>
class Array : public Composite<int> {
  public:
    template <class... Args>
    explicit Array(int nb_elems, Args... args) {
        for (int i = 0; i < nb_elems; i++) {
            component<T>(i, std::forward<Args>(args)...);
        }
    }
};

/*
====================================================================================================
  ~*~ ArraySet class ~*~
==================================================================================================*/
struct ArraySet {
    template <class Key, class... Keys, class Data>
    static void _connect(Assembly<Key>& assembly, _PortAddress<Keys...> array, const std::vector<Data>& data) {
        auto& arrayRef = assembly.template at<Assembly<int>>(array.address);
        for (int i = 0; i < static_cast<int>(arrayRef.size()); i++) {
            arrayRef.at(i).set(array.prop, data.at(i));
        }
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
struct ArrayOneToOne {
    template <class... Keys, class... Keys2>
    static void _connect(Assembly<>& a, _PortAddress<Keys...> array1, _Address<Keys2...> array2) {
        auto& ref1 = a.at<Assembly<int>>(array1.address);
        auto& ref2 = a.at<Assembly<int>>(array2);
        if (ref1.size() == ref2.size()) {
            for (int i = 0; i < static_cast<int>(ref1.size()); i++) {
                auto ptr = dynamic_cast<Interface*>(&ref2.at(i));
                ref1.at(i).set(array1.prop, ptr);
            }
        } else {
            TinycompoDebug e{"Array connection: mismatched sizes"};
            e << array1.address.to_string() << " has size " << ref1.size() << " while " << array2.to_string() << " has size "
              << ref2.size() << ".";
            e.fail();
        }
    }
};

/*
====================================================================================================
  ~*~ MultiUse class ~*~
The MultiUse class is a connector that connects (as if using the UseProvide connector) one port
of one component to every component in an array. This can be seen as a "multiple use" connector (the
reducer is the user in multiple use/provide connections). This class should be used as a template
parameter for Assembly::connect.
==================================================================================================*/
template <class Interface>
struct MultiUse {
    template <class... Keys, class... Keys2>
    static void _connect(Assembly<>& a, _PortAddress<Keys...> reducer, _Address<Keys2...> array) {
        auto& ref1 = a.at<Component>(reducer.address);
        auto& ref2 = a.at<Assembly<int>>(array);
        for (int i = 0; i < static_cast<int>(ref2.size()); i++) {
            auto ptr = dynamic_cast<Interface*>(&ref2.at(i));
            ref1.set(reducer.prop, ptr);
        }
    }
};

/*
====================================================================================================
  ~*~ MultiProvide class ~*~
==================================================================================================*/
template <class Interface>
struct MultiProvide {
    template <class... Keys, class... Keys2>
    static void _connect(Assembly<>& a, _PortAddress<Keys...> array, _Address<Keys2...> mapper) {
        try {
            for (int i = 0; i < static_cast<int>(a.at<Assembly<int>>(array.address).size()); i++) {
                a.at(Address(array.address, i)).set(array.prop, &a.at<Interface>(mapper));
            }
        } catch (...) {
            TinycompoDebug("<MultiProvide::_connect> There was an error while trying to connect components.").fail();
        }
    }
};

#endif  // TINYCOMPO_HPP

/* Copyright or © or Copr. Centre National de la Recherche Scientifique (CNRS) (2017/05/03)
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
#include <string.h>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

/*
====================================================================================================
  ~*~ Various forward-declarations and abstract interfaces ~*~
==================================================================================================*/
class Model;
class Assembly;
class _AssemblyGraph;

template <class T>  // this is an empty helper class that is used to pass T to the _ComponentBuilder
class _Type {};     // constructor below

struct _AbstractPort {
    virtual ~_AbstractPort() = default;
};

struct _AbstractAddress {};  // for identification of _Address types encountered in the wild
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
  _Port<Args...> derives from _AbstractPort which allows the storage of pointers to _Port by
  converting them to _AbstractPort*. These classes are for internal use by tinycompo and should not
  be seen by the user (as denoted by the underscore prefixes).
==================================================================================================*/
template <class... Args>
struct _Port : public _AbstractPort {
    std::function<void(Args...)> _set;

    _Port() = delete;

    template <class C>
    explicit _Port(C* ref, void (C::*prop)(Args...))
        : _set([=](const Args... args) { (ref->*prop)(std::forward<const Args>(args)...); }) {}

    template <class C, class Type>
    explicit _Port(C* ref, Type(C::*prop)) : _set([=](const Type arg) { ref->*prop = arg; }) {}
};

template <class Interface>
struct _ProvidePort : public _AbstractPort {
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
    std::map<std::string, std::unique_ptr<_AbstractPort>> _ports;
    std::string name{""};

  public:
    Component(const Component&) = delete;  // forbidding copy
    Component() = default;
    virtual ~Component() = default;

    virtual std::string _debug() const { return "Component"; };

    template <class C, class... Args>
    void port(std::string name, void (C::*prop)(Args...)) {  // port is a setter member function
        _ports[name] = std::unique_ptr<_AbstractPort>(
            static_cast<_AbstractPort*>(new _Port<const Args...>(dynamic_cast<C*>(this), prop)));
    }

    template <class C, class Arg>
    void port(std::string name, Arg(C::*prop)) {  // port is a data member
        _ports[name] =
            std::unique_ptr<_AbstractPort>(static_cast<_AbstractPort*>(new _Port<const Arg>(dynamic_cast<C*>(this), prop)));
    }

    template <class C, class Interface>
    void provide(std::string name, Interface* (C::*prop)()) {
        _ports[name] = std::unique_ptr<_AbstractPort>(
            static_cast<_AbstractPort*>(new _ProvidePort<Interface>(dynamic_cast<C*>(this), prop)));
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
  ~*~ _ComponentBuilder class ~*~
  A small class that is capable of storing a constructor call for any Component child class and
  execute said call later on demand. The class itself is not templated (allowing direct storage)
  but the constructor call is. This is an internal tinycompo class that should never be seen by
  the user (as denoted by the underscore prefix).
==================================================================================================*/
struct _ComponentBuilder {
    template <class T, class... Args>
    _ComponentBuilder(_Type<T>, Args... args)
        : _constructor([=]() { return std::unique_ptr<Component>(dynamic_cast<Component*>(new T(args...))); }),
          _class_name(TinycompoDebug::type<T>()) {}

    std::function<std::unique_ptr<Component>()> _constructor;  // stores the component constructor
    std::string _class_name{""};
};

/*
====================================================================================================
  ~*~ key_to_string ~*~
==================================================================================================*/
template <class Key>
std::string key_to_string(Key key) {
    std::stringstream ss;
    ss << key;
    return ss.str();
}

/*
====================================================================================================
  ~*~ Addresses ~*~
==================================================================================================*/
class Address {
    std::vector<std::string> keys;

    template <class Arg>
    void register_keys(Arg arg) {
        keys.push_back(key_to_string(arg));
    }

    template <class Arg, class... Args>
    void register_keys(Arg arg, Args... args) {
        register_keys(arg);
        register_keys(std::forward<Args>(args)...);
    }

  public:
    template <class... Keys>
    explicit Address(Keys... keys) {
        register_keys(std::forward<Keys>(keys)...);
    }

    explicit Address(const std::vector<std::string>& v) : keys(v) {}

    template <class Key>
    Address(const Address& address, Key key) : keys(address.keys) {
        keys.push_back(key_to_string(key));
    }

    std::string first() const { return keys.front(); }

    Address rest() const {
        std::vector<std::string> acc;
        for (unsigned int i = 1; i < keys.size(); i++) {
            acc.push_back(keys.at(i));
        }
        return Address(acc);
    }

    bool is_composite() const { return keys.size() > 1; }

    std::string to_string() const {
        return std::accumulate(keys.begin(), keys.end(), std::string(""),
                               [this](std::string acc, std::string key) { return ((acc == "") ? "" : acc + "_") + key; });
    }
};

struct PortAddress {
    std::string prop;
    Address address;

    template <class... Keys>
    PortAddress(const std::string& prop, Keys... keys) : prop(prop), address(std::forward<Keys>(keys)...) {}
};

/*
====================================================================================================
  ~*~ _Operation class ~*~
==================================================================================================*/
struct _Operation {
    template <class Connector, class... Args>
    _Operation(_Type<Connector>, Args... args)
        : _connect([=](Assembly& assembly) { Connector::_connect(assembly, args...); }) {}

    std::function<void(Assembly&)> _connect;
};

/*
====================================================================================================
  ~*~ Composite ~*~
==================================================================================================*/
struct Composite {
    static void contents(Model&) {}
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

    friend class _AssemblyGraph;

  public:
    _GraphAddress(const std::string& address, const std::string& port = "") : address(address), port(port) {}

    void print(std::ostream& os = std::cout) const { os << "->" << address << ((port == "") ? "" : ("." + port)); }
};

struct _Node {
    std::string name;
    std::string type;
    std::vector<_GraphAddress> neighbors;

    void neighbors_from_args() {}

    template <class Arg, class... Args>
    void neighbors_from_args(Arg, Args... args) {
        neighbors_from_args(args...);
    }

    template <class... Args>
    void neighbors_from_args(const Address& arg, Args... args) {
        neighbors.push_back(_GraphAddress(arg.to_string()));
        neighbors_from_args(args...);
    }

    template <class... Args>
    void neighbors_from_args(PortAddress arg, Args... args) {
        neighbors.push_back(_GraphAddress(arg.address.to_string(), arg.prop));
        neighbors_from_args(args...);
    }

    void print(std::ostream& os = std::cout, int tabs = 0) const {
        os << std::string(tabs, '\t') << ((name == "") ? "Connector" : "Component \"" + name + "\"") << " (" << type << ") ";
        for (auto& n : neighbors) {
            n.print(os);
            os << " ";
        }
        os << '\n';
    }
};

class _AssemblyGraph {
    std::vector<_Node> components;
    std::vector<_Node> connectors;
    std::map<std::string, _AssemblyGraph&> composites;

    friend class Model;

    std::string strip(std::string s) const {
        auto it = s.find('_');
        return s.substr(++it);
    }

    bool is_composite(const std::string& address) const {
        return std::accumulate(composites.begin(), composites.end(), false,
                               [this, address](bool acc, std::pair<std::string, _AssemblyGraph&> ref) {
                                   return acc || ref.second.is_composite(strip(address)) || (ref.first == address);
                               });
    }

  public:
    void to_dot(int tabs = 0, const std::string& name = "", std::ostream& os = std::cout) const {
        std::string prefix = name + (name == "" ? "" : "_");
        if (name == "") {  // toplevel
            os << std::string(tabs, '\t') << "graph g {\n";
        } else {
            os << std::string(tabs, '\t') << "subgraph cluster_" << name << " {\n";
        }
        for (auto& c : components) {
            os << std::string(tabs + 1, '\t') << prefix << c.name << " [label=\"" << c.name << "\\n(" << c.type
               << ")\" shape=component margin=0.15];\n";
        }
        int i = 0;
        for (auto& c : connectors) {
            std::string cname = "connect_" + prefix + std::to_string(i);
            os << std::string(tabs + 1, '\t') << cname << " [xlabel=\"" << c.type << "\" shape=point];\n";
            for (auto& n : c.neighbors) {
                os << std::string(tabs + 1, '\t') << cname << " -- "
                   << (is_composite(n.address) ? "cluster_" + prefix + n.address : prefix + n.address)
                   << (n.port == "" ? "" : "[xlabel=\"" + n.port + "\"]") << ";\n";
            }
            i++;
        }
        for (auto& c : composites) {
            c.second.to_dot(tabs + 1, prefix + c.first, os);
        }
        os << std::string(tabs, '\t') << "}\n";
    }

    void print(std::ostream& os = std::cout, int tabs = 0) const {
        for (auto& c : components) {
            c.print(os, tabs);
        }
        for (auto& c : connectors) {
            c.print(os, tabs);
        }
        for (auto& c : composites) {
            os << std::string(tabs, '\t') << "Composite " << c.first << " {\n";
            c.second.print(os, tabs + 1);
            os << std::string(tabs, '\t') << "}\n";
        }
    }

    std::vector<std::string> all_component_names(int depth = 0, const std::string& name = "") const {
        std::string prefix = name + (name == "" ? "" : "_");
        std::vector<std::string> result;
        for (auto& c : components) {            // local components
            result.push_back(prefix + c.name);  // stringified name
        }
        if (depth > 0) {
            for (auto& c : composites) {  // names from composites until a certain depth
                auto subresult = c.second.all_component_names(depth - 1, prefix + c.first);
                result.insert(result.end(), subresult.begin(), subresult.end());
            }
        }
        return result;
    }
};

/*
====================================================================================================
  ~*~ Model ~*~
==================================================================================================*/
class Model {
    friend class Assembly;  // to access internal data

  protected:
    std::map<std::string, _ComponentBuilder> components;
    std::vector<_Operation> operations;
    std::map<std::string, Model> composites;

    _AssemblyGraph representation;

  public:
    Model() = default;

    template <class T, class... Args>
    Model(_Type<T>, Args... args) {
        T::contents(*this, std::forward<Args>(args)...);
    }

    Model(const Model& other_model)
        : components(other_model.components),
          operations(other_model.operations),
          composites(other_model.composites),
          representation(other_model.representation) {
        representation.composites.clear();  // rebuild composite map in representation from scratch
        for (auto& c : composites) {
            representation.composites.emplace(std::piecewise_construct, std::forward_as_tuple(c.first),
                                              std::forward_as_tuple(c.second.get_representation()));
        }
    }

    template <class T, class... Args>
    void component(const Address& address, Args&&... args) {
        if (!address.is_composite()) {
            component<T>(address.first(), std::forward<Args>(args)...);
        } else {
            get_composite(address.first()).component<T>(address.rest(), std::forward<Args>(args)...);
        }
    }

    // horrible enable_if to avoid ambiguous call with version below
    template <class T, class CallKey, class... Args,
              typename = typename std::enable_if<!std::is_same<CallKey, Address>::value>::type>
    void component(CallKey key, Args&&... args) {
        if (!std::is_base_of<Component, T>::value) {
            TinycompoDebug("trying to declare a component that does not inherit from Component").fail();
        }
        std::string key_name = key_to_string(key);
        components.emplace(std::piecewise_construct, std::forward_as_tuple(key_name),
                           std::forward_as_tuple(_Type<T>(), std::forward<Args>(args)...));

        representation.components.push_back(_Node());
        representation.components.back().name = key_name;
        representation.components.back().type = TinycompoDebug::type<T>();
    }

    template <class T = Composite, class... Args>
    void composite(const Address& address, Args&&... args) {
        if (!address.is_composite()) {
            composite<T>(address.first(), std::forward<Args>(args)...);
        } else {
            get_composite(address.first()).composite<T>(address.rest(), std::forward<Args...>(args)...);
        }
    }

    template <class T = Composite, class CallKey, class... Args,
              typename = typename std::enable_if<!std::is_same<CallKey, Address>::value>::type>
    void composite(CallKey key, Args&&... args) {
        std::string key_name = key_to_string(key);

        composites.emplace(std::piecewise_construct, std::forward_as_tuple(key_name),
                           std::forward_as_tuple(_Type<T>(), std::forward<Args>(args)...));

        representation.composites.emplace(std::piecewise_construct, std::forward_as_tuple(key_name),
                                          std::forward_as_tuple(composites.at(key_name).get_representation()));
    }

    template <class Key>
    Model& get_composite(const Key& key) {
        std::string key_name = key_to_string(key);
        auto compositeIt = composites.find(key_name);
        return dynamic_cast<Model&>(compositeIt->second);
    }

    template <class C, class... Args>
    void connect(Args&&... args) {
        operations.emplace_back(_Type<C>(), args...);

        representation.connectors.push_back(_Node());
        representation.connectors.back().type = TinycompoDebug::type<C>();
        representation.connectors.back().neighbors_from_args(args...);
    }

    std::size_t size() const { return components.size() + composites.size(); }

    void dot(std::ostream& stream = std::cout) const { representation.to_dot(0, "", stream); }

    void dot_to_file(const std::string& fileName = "tmp.dot") const {
        std::ofstream file;
        file.open(fileName);
        dot(file);
    }

    void print_representation(std::ostream& os = std::cout, int tabs = 0) const { representation.print(os, tabs); }

    const _AssemblyGraph& get_representation() const { return representation; }
    _AssemblyGraph& get_representation() { return representation; }
};

/*
====================================================================================================
  ~*~ Assembly class ~*~
==================================================================================================*/
class Assembly : public Component {
  protected:  // used in mpi example
    std::map<std::string, std::unique_ptr<Component>> instances;
    Model internal_model;

  public:
    Assembly() = delete;
    explicit Assembly(Model& model, const std::string& name = "") : internal_model(model) {
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
            instances.emplace(c.first, std::unique_ptr<Component>(new Assembly(c.second, c.first)));
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

    bool is_composite(const Address& address) const {
        auto ptr = dynamic_cast<Assembly*>(&at(address));
        return ptr != nullptr;
    }

    template <class T = Component, class Key>
    T& at(Key key) const {
        std::string key_name = key_to_string(key);
        try {
            return dynamic_cast<T&>(*(instances.at(key_name).get()));
        } catch (std::out_of_range) {
            TinycompoDebug e{"<Assembly::at> Trying to access incorrect address"};
            e << "Address " << key_name << " does not exist. Existing addresses are:\n";
            for (auto& key : instances) {
                e << "  * " << key.first << "\n";
            }
            e.fail();
        }
    }

    template <class T = Component>
    T& at(const Address& address) const {
        if (!address.is_composite()) {
            return at<T>(address.first());
        } else {
            return at<Assembly>(address.first()).template at<T>(address.rest());
        }
    }

    const Model& model() const { return internal_model; }

    void print_all(std::ostream& os = std::cout) const {
        for (auto& i : instances) {
            os << i.first << ": " << i.second->_debug() << std::endl;
        }
    }

    std::set<std::string> all_keys() const {
        std::set<std::string> result;
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
    template <class... Args>
    static void _connect(Assembly& assembly, PortAddress component, Args... args) {
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
    static void _connect(Assembly& assembly, PortAddress user, Address provider) {
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
struct ListUse {  // TODO de-templatify
    template <class UserAddress, class ProviderAddress>
    static void _connect(Assembly& assembly, UserAddress user, ProviderAddress provider) {
        Use<Interface>::_connect(assembly, user, provider);
    }

    template <class UserAddress, class ProviderAddress, class... OtherProviderAddresses>
    static void _connect(Assembly& assembly, UserAddress user, ProviderAddress provider,
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
    static void _connect(Assembly& assembly, PortAddress user, PortAddress provider) {
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
struct Array : public Composite {
    template <class... Args>
    static void contents(Model& model, int nb_elems, Args... args) {
        for (int i = 0; i < nb_elems; i++) {
            model.component<T>(i, std::forward<Args>(args)...);
        }
    }
};

/*
====================================================================================================
  ~*~ ArraySet class ~*~
==================================================================================================*/
struct ArraySet {
    template <class... Keys, class Data>
    static void _connect(Assembly& assembly, PortAddress array, const std::vector<Data>& data) {
        auto& arrayRef = assembly.template at<Assembly>(array.address);
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
    static void _connect(Assembly& a, PortAddress array1, Address array2) {
        auto& ref1 = a.at<Assembly>(array1.address);
        auto& ref2 = a.at<Assembly>(array2);
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
    static void _connect(Assembly& a, PortAddress reducer, Address array) {
        auto& ref1 = a.at<Component>(reducer.address);
        auto& ref2 = a.at<Assembly>(array);
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
    static void _connect(Assembly& a, PortAddress array, Address mapper) {
        try {
            for (int i = 0; i < static_cast<int>(a.at<Assembly>(array.address).size()); i++) {
                a.at(Address(array.address, i)).set(array.prop, &a.at<Interface>(mapper));
            }
        } catch (...) {
            TinycompoDebug("<MultiProvide::_connect> There was an error while trying to connect components.").fail();
        }
    }
};

#endif  // TINYCOMPO_HPP

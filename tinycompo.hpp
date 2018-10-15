/* Copyright or © or Copr. Centre National de la Recherche Scientifique (CNRS) (2017/05/03)
Contributors:
- Vincent Lanore <vincent.lanore@gmail.com>

This software is a computer program whose purpose is to provide the necessary classes to write ligntweight component-based
c++ applications.

This software is governed by the CeCILL-B license under French law and abiding by the rules of distribution of free software.
You can use, modify and/ or redistribute the software under the terms of the CeCILL-B license as circulated by CEA, CNRS and
INRIA at the following URL "http://www.cecill.info".

As a counterpart to the access to the source code and rights to copy, modify and redistribute granted by the license, users
are provided only with a limited warranty and the software's author, the holder of the economic rights, and the successive
licensors have only limited liability.

In this respect, the user's attention is drawn to the risks associated with loading, using, modifying and/or developing or
reproducing the software by the user in light of its specific status of free software, that may mean that it is complicated
to manipulate, and that also therefore means that it is reserved for developers and experienced professionals having in-depth
computer knowledge. Users are therefore encouraged to load and test the software's suitability as regards their requirements
in conditions enabling the security of their systems and/or data to be ensured and, more generally, to use and operate it in
the same conditions as regards security.

The fact that you are presently reading this means that you have had knowledge of the CeCILL-B license and that you accept
its terms.*/

#ifndef TINYCOMPO_HPP
#define TINYCOMPO_HPP

#ifdef __GNUG__
#include <cxxabi.h>
#endif

#include <string.h>
#include <cassert>
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

namespace tc {

/*
=============================================================================================================================
  ~*~ Various forward-declarations and abstract interfaces ~*~
===========================================================================================================================*/
class Model;
class Assembly;
class Address;
class Component;
struct PortAddress;
class ComponentReference;
struct Composite;

struct Meta {};  // type tag for meta connectors

template <class T>  // this is an empty helper class that is used to pass T to the _ComponentBuilder
class _Type {};     // constructor below

struct _AbstractPort {
    virtual ~_AbstractPort() = default;
};

struct _AbstractProvidePort : public _AbstractPort {
    virtual Component* get_type_erased() = 0;
};

struct _AbstractDriver {
    virtual void go() = 0;
    virtual void set_refs(std::vector<Component*>) = 0;
};

struct _AbstractAddress {};  // for identification of _Address types encountered in the wild

using DirectedGraph = std::pair<std::set<std::string>, std::multimap<std::string, std::string>>;

/*
=============================================================================================================================
  ~*~ Debug ~*~
===========================================================================================================================*/
class TinycompoException : public std::exception {
    std::string message{""};
    std::vector<TinycompoException> context;

  public:
    TinycompoException(const std::string& init = "") : message{init} {}
    TinycompoException(const std::string& init, const TinycompoException& context_in)
        : message{init}, context({context_in}) {}
    const char* what() const noexcept override { return message.c_str(); }
};

class TinycompoDebug {  // bundle of static functions to help with debug messages
#ifdef __GNUG__
    static std::string demangle(const char* name) {
        int status{0};
        std::unique_ptr<char, void (*)(void*)> res{abi::__cxa_demangle(name, NULL, NULL, &status), std::free};
        return (status == 0) ? res.get() : name;
    }
#else
    static std::string demangle(const char* name) { return name; }
#endif

  public:
    template <class T>
    static std::string type() {  // display human-friendly typename
        return demangle(typeid(T).name());
    }

    template <class T1, class T2>
    static std::string list(const std::map<T1, T2>& structure) {  // bullet-pointed list of key names in a string
        std::stringstream acc;
        for (auto& e : structure) {
            acc << "  * " << e.first << '\n';
        }
        return acc.str();
    }
};

/*
=============================================================================================================================
  ~*~ _Port class ~*~
_Port<Args...> derives from _AbstractPort which allows the storage of pointers to _Port by converting them to _AbstractPort*.
These classes are for internal use by tinycompo and should not be seen by the user (as denoted by the underscore prefixes).
===========================================================================================================================*/
template <class... Args>
struct _Port : public _AbstractPort {
    std::function<void(Args...)> _set;

    _Port() = delete;

    template <class C>
    explicit _Port(C* ref, void (C::*prop)(Args...))
        : _set([=](const Args... args) { (ref->*prop)(std::forward<const Args>(args)...); }) {}

    template <class C, class Type>
    explicit _Port(C* ref, Type(C::*prop)) : _set([ref, prop](const Type arg) { ref->*prop = arg; }) {}
};

template <class Interface>
struct _ProvidePort : public _AbstractProvidePort {
    std::function<Interface*()> _get;
    Component* get_type_erased() override { return dynamic_cast<Component*>(_get()); }

    _ProvidePort() = delete;

    template <class C>
    explicit _ProvidePort(C* ref, Interface* (C::*prop)()) : _get([=]() { return (ref->*prop)(); }) {}

    _ProvidePort(Assembly& assembly, Address address);   // composite port, direct
    _ProvidePort(Assembly& assembly, PortAddress port);  // composite port, provide
};

/*
=============================================================================================================================
  ~*~ Component class ~*~
tinycompo components should always inherit from this class. It is mostly used as a base to be able to store pointers to child
class instances but also provides basic debugging methods and the infrastructure required to declare ports.
===========================================================================================================================*/
class Component {
    std::map<std::string, std::unique_ptr<_AbstractPort>> _ports;  // not meant to be accessible for users
    std::string name{""};                                          // accessible through get/set name accessors

    friend Assembly;

  public:
    /*
    =========================================================================================================================
      ~*~ Constructors ~*~  */

    void operator=(const Component&) = delete;  // forbidding assignation
    Component(const Component&) = delete;       // forbidding copy
    Component() = default;
    virtual ~Component() = default;

    /*
    =========================================================================================================================
      ~*~ Functions that can be overriden by user (lifecycle and debug) ~*~  */

    virtual std::string debug() const { return "Component"; }  // runtime overridable method for debug info (class&state)

    virtual void after_construct() {}  // called after component constructor but before connection

    virtual void after_connect() {}  // called after connections are all done

    /*
    =========================================================================================================================
      ~*~ Declaration of ports ~*~  */

    template <class C, class... Args>
    void port(std::string name, void (C::*prop)(Args...)) {  // case where the port is a setter member function
        _ports[name] = std::unique_ptr<_AbstractPort>(
            static_cast<_AbstractPort*>(new _Port<const Args...>(dynamic_cast<C*>(this), prop)));
    }

    template <class C, class Arg>
    void port(std::string name, Arg(C::*prop)) {  // case where the port is a data member
        _ports[name] =
            std::unique_ptr<_AbstractPort>(static_cast<_AbstractPort*>(new _Port<const Arg>(dynamic_cast<C*>(this), prop)));
    }

    template <class C, class Interface>
    void provide(std::string name, Interface* (C::*prop)()) {
        _ports[name] = std::unique_ptr<_AbstractPort>(
            static_cast<_AbstractPort*>(new _ProvidePort<Interface>(dynamic_cast<C*>(this), prop)));
    }

    /*
    =========================================================================================================================
      ~*~ Accessors to ports and name ~*~  */

    template <class... Args>
    void set(std::string name, Args... args) {    // no perfect forwarding to avoid references
        if (_ports.find(name) == _ports.end()) {  // there exists no port with this name
            throw TinycompoException{"Port name not found. Could not find port " + name + " in component " + debug() + "."};
        } else {  // there exists a port with this name
            auto ptr = dynamic_cast<_Port<const Args...>*>(_ports[name].get());
            if (ptr != nullptr)  // casting succeedeed
            {
                ptr->_set(std::forward<Args>(args)...);
            } else {  // casting failed, trying to provide useful error message
                throw TinycompoException("Setting property failed. Type " + TinycompoDebug::type<_Port<const Args...>>() +
                                         " does not seem to match port " + name + '.');
            }
        }
    }

    template <class Interface>
    Interface* get(std::string name) const {
        try {
            return dynamic_cast<_ProvidePort<Interface>*>(_ports.at(name).get())->_get();
        } catch (std::out_of_range) {
            throw TinycompoException("<Component::get<Interface>> Port name " + name + " not found. Existing ports are:\n" +
                                     TinycompoDebug::list(_ports));
        }
    }

    Component* get(std::string name) const {
        try {
            return dynamic_cast<_AbstractProvidePort*>(_ports.at(name).get())->get_type_erased();
        } catch (std::out_of_range) {
            throw TinycompoException("<Component::get> Port name " + name + " not found. Existing ports are:\n" +
                                     TinycompoDebug::list(_ports));
        }
    }

    void set_name(const std::string& n) { name = n; }
    std::string get_name() const { return name; }
};

/*
=============================================================================================================================
  ~*~ key_to_string ~*~
===========================================================================================================================*/
template <class Key>
std::string key_to_string(Key key) {
    std::stringstream ss;
    ss << key;
    return ss.str();
}

/*
=============================================================================================================================
  ~*~ Addresses ~*~
===========================================================================================================================*/
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
    Address() {}

    Address(const ComponentReference&);

    Address(const char* input) : Address(std::string(input)) {}

    Address(const std::string& input) {
        std::string copy = input;
        auto get_token = [&]() -> std::string {
            auto it = copy.find("__");
            std::string result;
            if (it != std::string::npos) {
                result = copy.substr(0, it);
                copy = copy.substr(++++it);
            } else {
                result = copy;
                copy = "";
            }
            return result;
        };
        std::string token = get_token();
        keys.push_back(token);
        while (true) {
            token = get_token();
            if (token == "") break;
            keys.push_back(token);
        }
    }

    template <class Key, class... Keys>
    Address(Key key, Keys... keys) {  // not explicit (how dangerous is this, really?)
        register_keys(key, std::forward<Keys>(keys)...);
    }

    explicit Address(const std::vector<std::string>& v) : keys(v) {}

    Address(const Address& a1, const Address& a2) : keys(a1.keys) {
        keys.insert(keys.end(), a2.keys.begin(), a2.keys.end());
    }

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
                               [](std::string acc, std::string key) { return ((acc == "") ? "" : acc + "__") + key; });
    }

    // for use as key in maps
    bool operator<(const Address& other_address) const {
        return std::lexicographical_compare(keys.begin(), keys.end(), other_address.keys.begin(), other_address.keys.end());
    }

    bool operator==(const Address& other_address) const { return keys == other_address.keys; }
};

struct PortAddress {
    std::string prop;
    Address address;

    template <class... Keys>
    PortAddress(const std::string& prop, Keys... keys) : prop(prop), address(std::forward<Keys>(keys)...) {}
    PortAddress(const std::string& prop, const Address& address) : prop(prop), address(address) {}

    bool operator==(const PortAddress& other_address) const {
        return prop == other_address.prop and address == other_address.address;
    }
};

/*
=============================================================================================================================
  ~*~ Graph representation classes ~*~
Small classes implementing a simple easily explorable graph representation for TinyCompo component assemblies.
===========================================================================================================================*/
struct _GraphAddress {
    std::string address;
    std::string port;

    _GraphAddress(const std::string& address, const std::string& port = "") : address(address), port(port) {}

    void print(std::ostream& os = std::cout) const { os << "->" << address << ((port == "") ? "" : ("." + port)); }
};

/*
=============================================================================================================================
  ~*~ _Operation class ~*~
===========================================================================================================================*/
class _Operation {
    template <class Functor, class... Args>
    void neighbors_from_args(Args... args) {  // populates the list of neighbors from arguments of the Connector
        helper1(Functor::_connect, args...);
    }

    template <class... Args, class... CArgs>
    void helper1(void (*)(Assembly&, Args...), CArgs... cargs) {
        void (*g)(Args...) = nullptr;  // Double recursion on connect call arguments (cargs) and on argument types of
        helper2(g, cargs...);          // _connect function (through the g pointer). This is necessary because call
    }                                  // arguments might have the wrong type (eg a string instead of an address).

    void helper2(void (*)()) {}

    template <class... Args, class CArg, class... CArgs>
    void helper2(void (*)(Address, Args...), CArg carg, CArgs... cargs) {
        neighbors.push_back(_GraphAddress(Address(carg).to_string()));
        void (*g)(Args...) = nullptr;
        helper2(g, cargs...);
    }

    template <class... Args, class... CArgs>
    void helper2(void (*)(PortAddress, Args...), PortAddress carg, CArgs... cargs) {
        neighbors.push_back(_GraphAddress(carg.address.to_string(), carg.prop));
        void (*g)(Args...) = nullptr;
        helper2(g, cargs...);
    }

    template <class Arg, class... Args, class CArg, class... CArgs>
    void helper2(void (*)(Arg, Args...), CArg, CArgs... cargs) {
        void (*g)(Args...) = nullptr;
        helper2(g, cargs...);
    }

  public:
    template <class Connector, class... Args>
    _Operation(_Type<Connector>, Args&&... args)
        : _connect([args...](Assembly& assembly) { Connector::_connect(assembly, args...); }),
          type(TinycompoDebug::type<Connector>()) {
        neighbors_from_args<Connector>(args...);
    }

    template <class Target, class Lambda>
    _Operation(Address address, _Type<Target>, Lambda lambda);  // def at end of file

    std::function<void(Assembly&)> _connect;

    // representation-related stuff
    std::string type;
    std::vector<_GraphAddress> neighbors;

    void print(std::ostream& os = std::cout, int tabs = 0) const {
        os << std::string(tabs, '\t') << "Connector (" << type << ") ";
        for (auto& n : neighbors) {
            n.print(os);
            os << " ";
        }
        os << '\n';
    }
};

/*
=============================================================================================================================
  ~*~ _ComponentBuilder class ~*~
A small class that is capable of storing a constructor call for any Component child class and execute said call later on
demand. The class itself is not templated (allowing direct storage) but the constructor call is. This is an internal
tinycompo class that should never be seen by the user (as denoted by the underscore prefix).
===========================================================================================================================*/
struct _ComponentBuilder {
    template <class T, class... Args>
    _ComponentBuilder(_Type<T>, const std::string& name, Args... args)
        : _constructor([=]() { return std::unique_ptr<Component>(dynamic_cast<Component*>(new T(args...))); }),
          type(TinycompoDebug::type<T>()),
          name(name) {}

    std::function<std::unique_ptr<Component>()> _constructor;  // stores the component constructor

    // representation-related stuff
    std::string type;
    std::string name;  // should it be removed (not very useful as its stored in a map by name)

    void print(std::ostream& os = std::cout, int tabs = 0) const {
        os << std::string(tabs, '\t') << "Component \"" << name << "\""
           << " (" << type << ")\n";
    }
};

/*
=============================================================================================================================
  ~*~ ComponentReference ~*~
Small class used to interact with an already-declared component without repeating its name everytime. This class allows the
chaining of declaration, eg : model.component(...).connect(...).connect(...).annotate(...)
===========================================================================================================================*/
class ComponentReference {
    Model& model_ref;
    Address component_address;
    friend Address;

  public:
    ComponentReference(Model& model_ref, const Address& component_address)
        : model_ref(model_ref), component_address(component_address) {}

    template <class T, class... Args>
    ComponentReference& connect(const std::string&, Args&&...);  // implemented at the end

    template <class... Args>
    ComponentReference& connect(Args&&...);  // implemented at the end

    template <class Lambda>
    ComponentReference& configure(Lambda lambda);  // implemented at the end

    template <class... Args>
    ComponentReference& set(const std::string&, Args&&...);  // implemented at the end
};

/*
=============================================================================================================================
  ~*~ _Driver ~*~
===========================================================================================================================*/
// invariant : Refs are all pointers to classes inheriting from Component
template <class... Refs>
class _Driver : public Component, public _AbstractDriver {
    std::function<void(Refs...)> instructions;
    std::tuple<Refs...> refs;

    // C++11 integer_sequence implementation :/
    template <int...>
    struct seq {};

    template <int N, int... S>
    struct gens : gens<N - 1, N - 1, S...> {};

    template <int... S>
    struct gens<0, S...> {
        typedef seq<S...> type;
    };

    // helper functions
    template <int... S>
    void call_helper(seq<S...>) {
        instructions(std::get<S>(refs)...);
    }

    template <int i>
    void set_ref_helper(std::vector<Component*>&) {}

    template <int i, class Head, class... Tail>
    void set_ref_helper(std::vector<Component*>& ref_values) {
        std::get<i>(refs) = dynamic_cast<Head>(ref_values.at(i));
        set_ref_helper<i + 1, Tail...>(ref_values);
    }

    // port to set the references (invariant : vector should have the same size as Refs)
    void set_refs(std::vector<Component*> ref_values) override { set_ref_helper<0, Refs...>(ref_values); }

    void go() override { call_helper(typename gens<sizeof...(Refs)>::type()); }

  public:
    _Driver(const std::function<void(Refs...)>& instructions) : instructions(instructions) {
        port("go", &_Driver::go);
        port("refs", &_Driver::set_refs);
    }
};

/*
=============================================================================================================================
  ~*~ Model ~*~
===========================================================================================================================*/
class Model {
    friend class Assembly;  // to access internal data
    friend class Introspector;

    // state of model
    std::map<std::string, _ComponentBuilder> components;
    std::vector<_Operation> operations;
    std::map<std::string, std::pair<Model, _ComponentBuilder>> composites;

    // helper functions
    std::string strip(std::string s) const {
        auto it = s.find("__");
        return s.substr(++++it);
    }

    template <class Lambda, class C>  // this helper extracts the component type from the lambda
    void configure_helper(Address address, Lambda lambda, void (Lambda::*)(C&) const) {
        operations.emplace_back(address, _Type<C>(), lambda);
    }

    template <class Lambda, class... Refs>  // this helper extracts the reference types from the lambda
    ComponentReference driver_helper(Address address, Lambda lambda, void (Lambda::*)(Refs...) const) {
        return component<_Driver<Refs...>>(address, lambda);
    }

    // helpers to select right component method
    using IsComponent = std::false_type;
    using IsComposite = std::true_type;
    using IsAddress = std::true_type;
    using IsNotAddress = std::false_type;
    using IsMeta = std::true_type;
    using IsConcrete = std::false_type;

    template <class T, class Whatever, class Whatever2, class... Args>
    ComponentReference component_call_helper(IsMeta, Whatever, Whatever2, Args&&... args) {
        return T::connect(*this, std::forward<Args>(args)...);
    }

    template <class T, class Whatever, class... Args>
    ComponentReference component_call_helper(IsConcrete, Whatever, IsAddress, const Address& address, Args&&... args) {
        if (!address.is_composite()) {
            component<T>(address.first(), std::forward<Args>(args)...);
        } else {
            get_composite(address.first()).component<T>(address.rest(), std::forward<Args>(args)...);
        }
        return ComponentReference(*this, address);
    }

    template <class T, class CallKey, class... Args>
    ComponentReference component_call_helper(IsConcrete, IsComponent, IsNotAddress, CallKey key, Args&&... args) {
        std::string key_name = key_to_string(key);
        components.emplace(std::piecewise_construct, std::forward_as_tuple(key_name),
                           std::forward_as_tuple(_Type<T>(), key_name, std::forward<Args>(args)...));
        return ComponentReference(*this, Address(key));
    }

    template <class T, class CallKey, class... Args>
    ComponentReference component_call_helper(IsConcrete, IsComposite, IsNotAddress, CallKey key, Args&&... args) {
        std::string key_name = key_to_string(key);

        Model m;
        T::contents(m, args...);

        composites.emplace(std::piecewise_construct, std::forward_as_tuple(key_name),
                           std::forward_as_tuple(std::piecewise_construct, std::forward_as_tuple(m),
                                                 std::forward_as_tuple(_Type<T>(), key_name)));
        return ComponentReference(*this, Address(key));
    }

    // helpers for connect method
    template <class C, class... Args>
    void connect_call_helper(IsConcrete, Args&&... args) {
        operations.emplace_back(_Type<C>(), std::forward<Args>(args)...);
    }

    template <class C, class... Args>
    void connect_call_helper(IsMeta, Args&&... args) {
        C::connect(*this, std::forward<Args>(args)...);
    }

    // helpers for introspection things
    std::vector<Address> all_addresses_helper(Address parent) const {
        std::vector<Address> result;
        for (auto&& c : components) {
            result.emplace_back(Address(parent, c.first));
        }
        for (auto&& c : composites) {
            auto recursive_result = c.second.first.all_addresses_helper(Address(parent, c.first));
            result.insert(result.end(), recursive_result.begin(), recursive_result.end());
        }
        return result;
    }

  public:
    Model() = default;  // when creating model from scratch

    template <class T, class... Args>
    Model(_Type<T>, Args... args) {  // when instantiating from composite content function
        T::contents(*this, std::forward<Args>(args)...);
    }
    /*
    =========================================================================================================================
    ~*~ Declaration functions ~*~  */

    template <class T, class MaybeAddress, class... Args>
    ComponentReference component(MaybeAddress address, Args... args) {
        return component_call_helper<T>(std::is_base_of<Meta, T>(), std::is_base_of<Composite, T>(),
                                        std::is_same<Address, MaybeAddress>(), address, args...);
    }

    ComponentReference composite(const Address& address) { return component<Composite>(address); }

    template <class C, class... Args>
    void connect(Args&&... args) {
        connect_call_helper<C>(std::is_base_of<Meta, C>(), std::forward<Args>(args)...);
    }

    template <class Lambda>
    void configure(Address address, Lambda lambda) {  // does not work with a function pointer (needs operator())
        configure_helper(address, lambda, &Lambda::operator());
    }

    template <class Lambda>
    ComponentReference driver(Address address, Lambda lambda) {
        return driver_helper(address, lambda, &Lambda::operator());
    }

    /*
    =========================================================================================================================
    ~*~ Getters / introspection ~*~  */

    Model& get_composite(const Address& address) {
        if (address.is_composite()) {
            return get_composite(address.first()).get_composite(address.rest());
        } else {
            std::string key_name = address.first();
            auto compositeIt = composites.find(key_name);
            if (compositeIt == composites.end()) {
                throw TinycompoException("Composite not found. Composite " + key_name +
                                         " does not exist. Existing composites are:\n" + TinycompoDebug::list(composites));
            } else {
                return dynamic_cast<Model&>(compositeIt->second.first);
            }
        }
    }

    const Model& get_composite(const Address& address) const {
        if (address.is_composite()) {
            return get_composite(address.first()).get_composite(address.rest());
        } else {
            std::string key_name = address.first();
            auto compositeIt = composites.find(key_name);
            if (compositeIt == composites.end()) {
                throw TinycompoException("Composite not found. Composite " + key_name +
                                         " does not exist. Existing composites are:\n" + TinycompoDebug::list(composites));
            } else {
                return dynamic_cast<const Model&>(compositeIt->second.first);
            }
        }
    }

    template <class T>
    bool has_type(const Address& address) const {
        if (address.is_composite()) {  // address is composite (several names)
            return get_composite(address.first()).has_type<T>(address.rest());
        } else {
            if (is_composite(address)) {  // non-composite address corresponds to a composite
                return false;             // composite don't have types
            } else {
                auto tmp_ptr = components.at(address.to_string())._constructor();
                return dynamic_cast<T*>(tmp_ptr.get()) != nullptr;
            }
        }
    }

    bool is_composite(const Address& address) const {
        if (address.is_composite()) {
            return get_composite(address.first()).is_composite(address.rest());
        } else {
            bool result = false;
            for (auto& c : composites) {
                result = result or c.first == address.first() or c.second.first.is_composite(address.first());
            }
            return result;
        }
    }

    bool exists(const Address& address) const {
        if (address.is_composite()) {
            return get_composite(address.first()).exists(address.rest());
        } else {
            return components.count(address.first()) != 0 or composites.count(address.first()) != 0;
        }
    }

    std::size_t size() const { return components.size() + composites.size(); }

    void dot(std::ostream& stream = std::cout) const { to_dot(0, "", stream); }

    void dot_to_file(const std::string& fileName = "tmp.dot") const {
        std::ofstream file;
        file.open(fileName);
        dot(file);
    }

    DirectedGraph get_digraph() const {
        std::set<std::string> nodes;
        std::multimap<std::string, std::string> edges;
        for (auto c : operations) {
            // if connector is of the form (PortAddress, Address)
            if ((c.neighbors.size() == 2) and (c.neighbors[0].port != "") and (c.neighbors[1].port == "")) {
                edges.insert(make_pair(c.neighbors[0].address, c.neighbors[1].address));
                nodes.insert(c.neighbors[0].address);
                nodes.insert(c.neighbors[1].address);
            }
        }
        return make_pair(nodes, edges);
    }

    void to_dot(int tabs = 0, const std::string& name = "", std::ostream& os = std::cout) const {
        std::string prefix = name + (name == "" ? "" : "__");
        if (name == "") {  // toplevel
            os << std::string(tabs, '\t') << "graph g {\n";
        } else {
            os << std::string(tabs, '\t') << "subgraph cluster_" << name << " {\n";
        }
        for (auto& c : components) {
            os << std::string(tabs + 1, '\t') << prefix << c.first << " [label=\"" << c.first << "\\n(" << c.second.type
               << ")\" shape=component margin=0.15];\n";
        }
        int i = 0;
        for (auto& c : operations) {
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
            c.second.first.to_dot(tabs + 1, prefix + c.first, os);
        }
        os << std::string(tabs, '\t') << "}\n";
    }

    void print(std::ostream& os = std::cout, int tabs = 0) const {
        for (auto& c : components) {
            c.second.print(os, tabs);
        }
        for (auto& c : operations) {
            c.print(os, tabs);
        }
        for (auto& c : composites) {
            os << std::string(tabs, '\t') << "Composite " << c.first << " {\n";
            c.second.first.print(os, tabs + 1);
            os << std::string(tabs, '\t') << "}\n";
        }
    }

    std::vector<Address> all_addresses() const {
        std::vector<Address> result;
        for (auto&& c : components) {
            result.emplace_back(c.first);
        }
        for (auto&& c : composites) {
            auto recursive_result = c.second.first.all_addresses_helper(c.first);
            result.insert(result.end(), recursive_result.begin(), recursive_result.end());
        }
        return result;
    }

    std::vector<Address> all_addresses(const Address& address) const { return get_composite(address).all_addresses(); }

    std::vector<std::string> all_component_names(int depth = 0, bool include_composites = false,
                                                 const std::string& name = "") const {
        std::string prefix = name + (name == "" ? "" : "__");
        std::vector<std::string> result;
        for (auto& c : components) {             // local components
            result.push_back(prefix + c.first);  // stringified name
        }
        if (include_composites) {
            for (auto& c : composites) {
                result.push_back(prefix + c.first);
            }
        }
        if (depth > 0) {
            for (auto& c : composites) {  // names from composites until a certain depth
                auto subresult = c.second.first.all_component_names(depth - 1, include_composites, prefix + c.first);
                result.insert(result.end(), subresult.begin(), subresult.end());
            }
        }
        return result;
    }
};

/*
=============================================================================================================================
  ~*~ Introspector ~*~
===========================================================================================================================*/
class Introspector {
    Model& m;

  public:
    Introspector(Model& m) : m(m) {}

    /*
    =========================================================================================================================
    ~*~ Size functions ~*~  */
    size_t nb_components() const { return m.components.size() + m.composites.size(); }
    size_t nb_operations() const { return m.operations.size(); }
    size_t deep_nb_components() const {
        size_t result = m.components.size();
        for (auto composite : m.composites) {
            Introspector i(composite.second.first);
            result += i.deep_nb_components();
        }
        return result;
    }
    size_t deep_nb_operations() const {
        size_t result = nb_operations();
        for (auto composite : m.composites) {
            Introspector i(composite.second.first);
            result += i.deep_nb_operations();
        }
        return result;
    }

    /*
    =========================================================================================================================
    ~*~ Topology-related functions ~*~  */
    std::vector<Address> components() const {
        std::vector<Address> result;
        for (auto component : m.components) {
            result.emplace_back(component.first);
        }
        for (auto composite : m.composites) {
            result.emplace_back(composite.first);
        }
        return result;
    }

    std::vector<std::pair<PortAddress, Address>> oriented_binary_operations() const {
        std::vector<std::pair<PortAddress, Address>> result;
        for (auto operation : m.operations) {
            auto& n = operation.neighbors;
            if (n.size() == 2 and n.at(0).port != "" and n.at(1).port == "") {
                result.emplace_back(PortAddress(n.at(0).port, n.at(0).address), Address(n.at(1).address));
            }
        }
        return result;
    }
};

/*
=============================================================================================================================
  ~*~ InstanceSet ~*~
  An object rezpresenting a set of instantiated components. To be obtained via Assembly.
===========================================================================================================================*/
template <class C>
class InstanceSet {
    std::vector<Address> _names;
    std::vector<C*> _pointers;

  public:
    void push_back(Address address, C* pointer) {
        _names.push_back(address);
        _pointers.push_back(pointer);
    }

    void combine(const InstanceSet<C>& other) {
        _names.insert(_names.end(), other._names.begin(), other._names.end());
        _pointers.insert(_pointers.end(), other._pointers.begin(), other._pointers.end());
    }

    const std::vector<Address>& names() const { return _names; }
    const std::vector<C*>& pointers() const { return _pointers; }
};

/*
=============================================================================================================================
  ~*~ Assembly class ~*~
===========================================================================================================================*/
class Assembly : public Component {
    std::map<std::string, std::unique_ptr<Component>> instances;
    Model internal_model;

    friend Composite;

    void build() {
        for (auto& c : internal_model.components) {
            instances.emplace(c.first, std::unique_ptr<Component>(c.second._constructor()));
            std::stringstream ss;
            ss << get_name() << ((get_name() != "") ? "__" : "") << c.first;
            instances.at(c.first).get()->set_name(ss.str());
        }
        for (auto& c : internal_model.composites) {
            std::stringstream ss;
            ss << get_name() << ((get_name() != "") ? "__" : "") << c.first;
            auto it = instances.emplace(c.first, std::unique_ptr<Component>(c.second.second._constructor())).first;
            auto& ref = dynamic_cast<Assembly&>(*(*it).second.get());
            ref.set_name(ss.str());
            ref.instantiate_from(c.second.first);
        }
        for (auto& i : instances) {
            i.second->after_construct();
        }
        for (auto& o : internal_model.operations) {
            o._connect(*this);
        }
        for (auto& i : instances) {
            i.second->after_connect();
        }
    }

  public:
    Assembly() : internal_model(Model()) {}

    explicit Assembly(Model& model, const std::string& name = "") : internal_model(model) {
        set_name(name);
        build();
    }

    void instantiate_from(Model model) {
        internal_model = model;
        instantiate();
    }

    void instantiate() {
        instances.clear();
        build();
    }

    std::string debug() const override {
        std::stringstream ss;
        ss << "Composite {\n";
        print(ss);
        ss << "}";
        return ss.str();
    }

    std::size_t size() const { return instances.size(); }

    template <class C>
    bool derives_from(const Address& address) const {
        auto ptr = dynamic_cast<C*>(&at(address));
        return ptr != nullptr;
    }

    bool is_composite(const Address& address) const { return derives_from<Assembly>(address); }

    template <class T = Component, class Key>
    T& at(Key key) const {
        std::string key_name = key_to_string(key);
        try {
            return dynamic_cast<T&>(*(instances.at(key_name).get()));
        } catch (std::out_of_range) {
            throw TinycompoException("<Assembly::at> Trying to access incorrect address. Address " + key_name +
                                     " does not exist. Existing addresses are:\n" + TinycompoDebug::list(instances));
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

    template <class T = Component>
    T& at(const PortAddress& port_address) const {
        auto& compo_ref = at(port_address.address);
        return *compo_ref.get<T>(port_address.prop);
    }

    Model& get_model() { return internal_model; }

    void print(std::ostream& os = std::cout) const {
        for (auto& i : instances) {
            os << i.first << ": " << i.second->debug() << std::endl;
        }
    }

    template <class... Args>
    void call(const PortAddress& port, Args... args) const {
        at(port.address).set(port.prop, std::forward<Args>(args)...);
    }

    template <class Key, class... Args>
    void call(const Key& key, const std::string& prop, Args... args) const {
        at(key).set(prop, std::forward<Args>(args)...);
    }

    template <class Interface>
    void provide(const std::string& prop_name, const Address& address) {
        _ports[prop_name] =
            std::unique_ptr<_AbstractPort>(static_cast<_AbstractPort*>(new _ProvidePort<Interface>(*this, address)));
    }

    template <class Interface>
    void provide(const std::string prop_name, const PortAddress& address) {
        _ports[prop_name] =
            std::unique_ptr<_AbstractPort>(static_cast<_AbstractPort*>(new _ProvidePort<Interface>(*this, address)));
    }

    template <class T = Component>
    InstanceSet<T> get_all_helper(const Address parent = Address()) {
        InstanceSet<T> result;
        auto all_addresses = internal_model.all_addresses();
        for (auto&& address : all_addresses) {
            auto ptr = dynamic_cast<T*>(&at<Component>(address));
            if (ptr != nullptr) {
                result.push_back(Address(parent, address), ptr);
            }
        }
        return result;
    }

    template <class T = Component>
    InstanceSet<T> get_all() {
        return get_all_helper<T>(Address());
    }

    template <class T>
    InstanceSet<T> get_all(std::set<Address> composites_and_components, const Address& point_of_view = Address()) {
        InstanceSet<T> result;
        for (auto&& compo : composites_and_components) {
            if (is_composite(compo)) {
                result.combine(get_all<T>(compo, point_of_view));
            } else {
                result.push_back(compo, &at<T>(compo));
            }
        }
        return result;
    }

    template <class T>
    InstanceSet<T> get_all(const Address& composite, const Address& point_of_view = Address("invalid")) {
        Address pov = (point_of_view == Address("invalid")) ? composite : point_of_view;
        if (composite == Address()) {
            return get_all_helper<T>();
        } else if (pov == Address()) {
            return at<Assembly>(composite).get_all_helper<T>(composite);
        } else {
            return at<Assembly>(pov.first()).get_all<T>(composite.rest(), pov.rest());
        }
    }
};

/*
=============================================================================================================================
  ~*~ Composite ~*~
===========================================================================================================================*/
struct Composite : public Assembly {
    static void contents(Model&) {}  // useful for empty composites
};

template <class C, class... Args>
void instantiate_composite(C& c, Args&&... args) {
    Model m;
    C::contents(m, std::forward<Args>(args)...);
    c.instantiate_from(m);
    c.after_construct();
}

/*
=============================================================================================================================
  ~*~ Set class ~*~
===========================================================================================================================*/
template <class... Args>
struct Set {
    static void _connect(Assembly& assembly, PortAddress component, Args... args) {
        assembly.at(component.address).set(component.prop, std::forward<Args>(args)...);
    }
};

/*
=============================================================================================================================
  ~*~ Use class ~*~
UseProvide is a "connector class", ie a functor that can be passed as template parameter to Assembly::connect. This
particular connector implements the "use/provide" connection, ie setting a port of one component (the user) to a pointer
to an interface of another (the provider). This class should be used as-is to declare assembly connections.
===========================================================================================================================*/
template <class Interface>
struct Use {
    static void _connect(Assembly& assembly, PortAddress user, Address provider) {
        auto& ref_user = assembly.at(user.address);
        auto& ref_provider = assembly.template at<Interface>(provider);
        ref_user.set(user.prop, &ref_provider);
    }
};

/*
=============================================================================================================================
  ~*~ UseProvide class ~*~
===========================================================================================================================*/
template <class Interface>
struct UseProvide {
    static void _connect(Assembly& assembly, PortAddress user, PortAddress provider) {
        auto& ref_user = assembly.at(user.address);
        auto& ref_provider = assembly.at(provider.address);
        ref_user.set(user.prop, ref_provider.template get<Interface>(provider.prop));
    }
};

/*
=============================================================================================================================
  ~*~ Array class ~*~
===========================================================================================================================*/
template <class T>
struct Array : public Composite {
    using Composite::Composite;

    template <class... Args>
    static void contents(Model& model, int nb_elems, Args&&... args) {
        for (int i = 0; i < nb_elems; i++) {
            model.component<T>(i, std::forward<Args>(args)...);
        }
    }
};

/*
=============================================================================================================================
  ~*~ ArraySet class ~*~
===========================================================================================================================*/
template <class Data>
struct ArraySet {
    static void _connect(Assembly& assembly, PortAddress array, const std::vector<Data>& data) {
        auto& arrayRef = assembly.template at<Assembly>(array.address);
        for (int i = 0; i < static_cast<int>(arrayRef.size()); i++) {
            arrayRef.at(i).set(array.prop, data.at(i));
        }
    }
};
/*
=============================================================================================================================
  ~*~ ArrayOneToOne class ~*~
This is a connector that takes two arrays with identical sizes and connects (as if using the UseProvide connector) every
i-th element in array1 to its corresponding element in array2 (ie, the i-th element in array2). This class should be used as
a template parameter for Assembly::connect.
===========================================================================================================================*/
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
            throw TinycompoException{"Array connection: mismatched sizes. " + array1.address.to_string() + " has size " +
                                     std::to_string(ref1.size()) + " while " + array2.to_string() + " has size " +
                                     std::to_string(ref2.size()) + '.'};
        }
    }
};

/*
=============================================================================================================================
  ~*~ MultiUse class ~*~
The MultiUse class is a connector that connects (as if using the Use connector) one port of one component to every component
in an array. This can be seen as a "multiple use" connector (the reducer is the user in multiple use/provide connections).
This class should be used as a template parameter for Assembly::connect.
===========================================================================================================================*/
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
=============================================================================================================================
  ~*~ MultiProvide class ~*~
===========================================================================================================================*/
template <class Interface>
struct MultiProvide {
    static void _connect(Assembly& a, PortAddress array, Address mapper) {
        try {
            for (int i = 0; i < static_cast<int>(a.at<Assembly>(array.address).size()); i++) {
                a.at(Address(array.address, i)).set(array.prop, &a.at<Interface>(mapper));
            }
        } catch (...) {
            throw TinycompoException("<MultiProvide::_connect> There was an error while trying to connect components.");
        }
    }
};

/*
=============================================================================================================================
  ~*~ DriverConnect class ~*~
===========================================================================================================================*/
template <class... Addresses>
struct DriverConnect {
    static void ref_gathering_helper(Assembly&, std::vector<Component*>&) {}

    template <class Head, class... Tail>
    static void ref_gathering_helper(Assembly& a, std::vector<Component*>& result, Head head, Tail... tail) {
        result.push_back(&a.at(Address(head)));
        ref_gathering_helper(a, result, tail...);
    }

    template <class... Tail>
    static void ref_gathering_helper(Assembly& a, std::vector<Component*>& result, PortAddress head, Tail... tail) {
        auto provided_port = a.at(head.address).get(head.prop);
        result.push_back(provided_port);
        ref_gathering_helper(a, result, tail...);
    }

    static void _connect(Assembly& a, Address driver, Addresses... addresses) {
        std::vector<Component*> result;
        ref_gathering_helper(a, result, addresses...);
        a.at<_AbstractDriver>(driver).set_refs(result);
    }
};

/*
=============================================================================================================================
  ~*~ Out-of-order implementations ~*~
===========================================================================================================================*/

template <class Target, class Lambda>
inline _Operation::_Operation(Address address, _Type<Target>, Lambda lambda)
    : _connect([lambda, address](Assembly& a) { lambda(a.at<Target>(address)); }), type("lambda") {}

// Address method that depends on ComponentReference
inline Address::Address(const ComponentReference& ref) { keys = ref.component_address.keys; }

// ComponentReference methods that depend on Model
template <class T, class... Args>
inline ComponentReference& ComponentReference::connect(const std::string& port, Args&&... args) {
    model_ref.connect<T>(PortAddress(port, component_address), std::forward<Args>(args)...);
    return *this;
}

template <class... Args>
ComponentReference& ComponentReference::connect(Args&&... args) {
    model_ref.connect<DriverConnect<Args...>>(component_address, std::forward<Args>(args)...);
    return *this;
}

template <class Lambda>
inline ComponentReference& ComponentReference::configure(Lambda lambda) {
    model_ref.configure(component_address, lambda);
    return *this;
}

template <class... Args>
inline ComponentReference& ComponentReference::set(const std::string& port, Args&&... args) {
    model_ref.connect<Set<Args...>>(PortAddress(port, component_address), std::forward<Args>(args)...);
    return *this;
}

// implementation of _ProvidePort methods that depended on Assembly interface
template <class Interface>
inline _ProvidePort<Interface>::_ProvidePort(Assembly& assembly, Address address)
    : _get([&assembly, address]() { return &assembly.at<Interface>(address); }) {}

template <class Interface>
inline _ProvidePort<Interface>::_ProvidePort(Assembly& assembly, PortAddress port)
    : _get([&assembly, port]() { return assembly.at<Component>(port.address).get<Interface>(port.prop); }) {}

}  // namespace tc

#endif  // TINYCOMPO_HPP

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
template <class Key>
class Model;

template <class>
class _AssemblyGraph;

template <class>
class Assembly;

struct _AssemblyGraphInterface {
    virtual void print(std::ostream&, int) const = 0;
    virtual bool is_composite(const std::string&) const = 0;
    virtual void to_dot(int, const std::string&, std::ostream& = std::cout) const = 0;
    virtual std::vector<std::string> all_component_names(int = 0, const std::string& = "") const = 0;
};

struct _ModelInterface {
    virtual void print_representation(std::ostream&, int) const = 0;
    virtual const _AssemblyGraphInterface& get_representation() const = 0;
    virtual _AssemblyGraphInterface& get_representation() = 0;
};

template <class T>  // this is an empty helper class that is used to pass T to the _ComponentBuilder
class _Type {};     // constructor below

struct _AbstractPort {
    virtual ~_AbstractPort() = default;
};

struct _AbstractAddress {};  // for identification of _Address types encountered in the wild

struct _AbstractComposite {  // inheritance-only class
    virtual ~_AbstractComposite() = default;
};

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
    void port(std::string name, void (C::*prop)(Args...)) {
        _ports[name] = std::unique_ptr<_AbstractPort>(
            static_cast<_AbstractPort*>(new _Port<const Args...>(dynamic_cast<C*>(this), prop)));
    }

    template <class C, class Arg>
    void port(std::string name, Arg(C::*prop)) {
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
  ~*~ _Key ~*~
==================================================================================================*/
template <class Type>
class _Key {
    Type value;

  public:
    using actual_type = Type;
    explicit _Key(Type value) : value(value) {}
    explicit _Key(const std::string& s) {
        std::stringstream ss(s);
        ss >> value;
    }
    Type get() const { return value; }
    void set(Type new_value) { value = new_value; }
    std::string to_string() {
        std::stringstream ss;
        ss << value;
        return ss.str();
    }
};

template <>
class _Key<std::string> {  // simpler than general case
    std::string value;

  public:
    using actual_type = std::string;
    explicit _Key(const std::string& value) : value(value) {}
    std::string get() const { return value; }
    void set(std::string new_value) { value = new_value; }
    std::string to_string() { return get(); }
};

template <>
class _Key<const char*> {  // special case: type needs to actually be string
    std::string value;

  public:
    using actual_type = std::string;
    explicit _Key(const std::string& value) : value(value) {}
    std::string get() const { return value; }
    void set(std::string new_value) { value = new_value; }
    std::string to_string() { return get(); }
};

/*
====================================================================================================
  ~*~ Addresses ~*~
==================================================================================================*/
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
struct _Operation {
    template <class Connector, class... Args>
    _Operation(_Type<Connector>, Args... args) : _connect([=](A& assembly) { Connector::_connect(assembly, args...); }) {}

    std::function<void(A&)> _connect;
};

/*
====================================================================================================
  ~*~ Composite ~*~
==================================================================================================*/
template <class Key = std::string>
class Composite : public Model<Key>, public _AbstractComposite {};

/*
====================================================================================================
  ~*~ _CompositeBuilder ~*~
  A small template-less class capable of constructing or copying composites without knowing their
  key type. Uses the _AbstractComposite class to store composites.
==================================================================================================*/
class _CompositeBuilder {
    std::unique_ptr<_AbstractComposite> ptr;
    std::function<_AbstractComposite*()> _clone;

  public:
    std::function<Component*(std::string)> _constructor;

    _CompositeBuilder() = delete;

    template <class T, class... Args>
    explicit _CompositeBuilder(_Type<T>, Args&&... args)
        : ptr(std::unique_ptr<_AbstractComposite>(new T(std::forward<Args>(args)...))),
          _clone([=]() { return static_cast<_AbstractComposite*>(new T(dynamic_cast<T&>(*ptr.get()))); }),
          _constructor([=](std::string s) {
              return static_cast<Component*>(new Assembly<typename T::KeyType>(dynamic_cast<T&>(*ptr.get()), s));
          }) {}

    _CompositeBuilder(const _CompositeBuilder& other)
        : ptr(other._clone()), _clone(other._clone), _constructor(other._constructor) {}

    _AbstractComposite* get() { return ptr.get(); }

    _ModelInterface& get_model_ref() { return *dynamic_cast<_ModelInterface*>(ptr.get()); }
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

    template <class>
    friend class _AssemblyGraph;

  public:
    _GraphAddress(const std::string& address, const std::string& port = "") : address(address), port(port) {}

    void print(std::ostream& os = std::cout) const { os << "->" << address << ((port == "") ? "" : ("." + port)); }
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

    void print(std::ostream& os = std::cout, int tabs = 0) const {
        os << std::string(tabs, '\t') << ((name == "") ? "Connector" : "Component \"" + name + "\"") << " (" << type << ") ";
        for (auto& n : neighbors) {
            n.print(os);
            os << " ";
        }
        os << '\n';
    }
};

template <class Key>
class _AssemblyGraph : public _AssemblyGraphInterface {
    std::vector<_Node<Key>> components;
    std::vector<_Node<Key>> connectors;
    std::map<Key, _AssemblyGraphInterface&> composites;

    template <class>
    friend class Model;

    std::string strip(std::string s) const {
        auto it = s.find('_');
        return s.substr(++it);
    }

    bool is_composite(const std::string& address) const override {
        return std::accumulate(composites.begin(), composites.end(), false,
                               [this, address](bool acc, std::pair<Key, _AssemblyGraphInterface&> ref) {
                                   return acc || ref.second.is_composite(strip(address)) ||
                                          (_Key<Key>(ref.first).to_string() == address);
                               });
    }

  public:
    void to_dot(int tabs = 0, const std::string& name = "", std::ostream& os = std::cout) const override {
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
            c.second.to_dot(tabs + 1, prefix + _Key<Key>(c.first).to_string(), os);
        }
        os << std::string(tabs, '\t') << "}\n";
    }

    void print(std::ostream& os = std::cout, int tabs = 0) const override {
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

    std::vector<std::string> all_component_names(int depth = 0, const std::string& name = "") const override {
        std::string prefix = name + (name == "" ? "" : "_");
        std::vector<std::string> result;
        for (auto& c : components) {            // local components
            result.push_back(prefix + c.name);  // stringified name
        }
        if (depth > 0) {
            for (auto& c : composites) {  // names from composites until a certain depth
                auto subresult = c.second.all_component_names(depth - 1, prefix + _Key<Key>(c.first).to_string());
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
template <class Key = std::string>
class Model : public _ModelInterface {
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
            auto ptr = dynamic_cast<Model<typename _Key<Key2>::actual_type>*>(compositeIt->second.get());
            if (ptr == nullptr) {
                TinycompoDebug e("key type does not match composite key type");
                e << "Key has type " << TinycompoDebug::type<typename _Key<Key2>::actual_type>() << " while composite "
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

    template <class>
    friend class Assembly;  // to access internal data

    template <class>
    friend class Model;  // to call _route

  protected:
    std::map<Key, _ComponentBuilder> components;
    std::vector<_Operation<Assembly<Key>, Key>> operations;
    std::map<Key, _CompositeBuilder> composites;

    _AssemblyGraph<Key> representation;

  public:
    using KeyType = Key;

    Model() = default;
    Model(const Model& other_model)
        : components(other_model.components),
          operations(other_model.operations),
          composites(other_model.composites),
          representation(other_model.representation) {
        representation.composites.clear();  // rebuild composite map in representation from scratch
        for (auto& c : composites) {
            representation.composites.emplace(
                std::piecewise_construct, std::forward_as_tuple(c.first),
                std::forward_as_tuple(dynamic_cast<_ModelInterface*>(c.second.get())->get_representation()));
        }
    }

    // horrible enable_if to avoid ambiguous call with version below
    template <class T, class CallKey, class... Args,
              class = typename std::enable_if<!std::is_base_of<_AbstractAddress, CallKey>::value>::type>
    void component(CallKey address, Args&&... args) {
        if (!std::is_base_of<Component, T>::value) {
            TinycompoDebug("trying to declare a component that does not inherit from Component").fail();
        }
        _Key<CallKey> key(address);
        components.emplace(std::piecewise_construct, std::forward_as_tuple(key.get()),
                           std::forward_as_tuple(_Type<T>(), std::forward<Args>(args)...));

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

        representation.composites.emplace(
            std::piecewise_construct, std::forward_as_tuple(key),
            std::forward_as_tuple(dynamic_cast<_ModelInterface*>(composites.at(key).get())->get_representation()));
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

        representation.connectors.push_back(_Node<Key>());
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

    void print_representation(std::ostream& os = std::cout, int tabs = 0) const override { representation.print(os, tabs); }

    const _AssemblyGraphInterface& get_representation() const override {
        return static_cast<const _AssemblyGraphInterface&>(representation);
    }

    _AssemblyGraphInterface& get_representation() override { return static_cast<_AssemblyGraphInterface&>(representation); }
};

/*
====================================================================================================
  ~*~ Assembly class ~*~
==================================================================================================*/
template <class Key = std::string>
class Assembly : public Component {
  protected:  // used in mpi example
    std::map<Key, std::unique_ptr<Component>> instances;
    Model<Key> internal_model;

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
        return at<Assembly<typename _Key<Key2>::actual_type>>(address.key.get()).template at<T>(address.rest);
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

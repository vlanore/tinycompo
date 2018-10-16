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

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "test_utils.hpp"
using namespace std;

/*
=============================================================================================================================
  ~*~ Debug ~*~
===========================================================================================================================*/
TEST_CASE("Demangling test") {
    CHECK(TinycompoDebug::type<void*(int, int)>() == "void* (int, int)");
    CHECK(TinycompoDebug::type<_Port<MyCompo>>() == "tc::_Port<MyCompo>");
}

TEST_CASE("Exception overhaul tests") {
    TinycompoException e1("An error occured");
    TinycompoException e2("Something went wrong in context:", e1);
    CHECK(string(e1.what()) == "An error occured");
    CHECK(string(e2.what()) == "Something went wrong in context:");
}

/*
=============================================================================================================================
  ~*~ _Port ~*~
===========================================================================================================================*/
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
    auto ptr = static_cast<_AbstractPort*>(new _Port<int, int>{&compo, &MyCompo::setIJ});
    auto ptr2 = dynamic_cast<_Port<int, int>*>(ptr);
    REQUIRE(ptr2 != nullptr);
    ptr2->_set(3, 4);
    CHECK(compo.i == 3);
    CHECK(compo.j == 4);
    delete ptr;
}

/*
=============================================================================================================================
  ~*~ Component ~*~
===========================================================================================================================*/
TEST_CASE("Component tests.") {
    MyCompo compo{};
    // MyCompo compo2 = compo; // does not work because Component copy is forbidden (intentional)
    CHECK(compo.debug() == "MyCompo");
    compo.set("myPort", 17, 18);
    CHECK(compo.i == 17);
    CHECK(compo.j == 18);
    TINYCOMPO_TEST_ERRORS { compo.set("myPort", true); }
    TINYCOMPO_TEST_ERRORS_END("Setting property failed. Type tc::_Port<bool const> does not seem to match port myPort.");
    TINYCOMPO_TEST_MORE_ERRORS { compo.set("badPort", 1, 2); }
    TINYCOMPO_TEST_ERRORS_END("Port name not found. Could not find port badPort in component MyCompo.");
}

TEST_CASE("Component without debug") {
    struct MyBasicCompo : public Component {};
    MyBasicCompo compo{};
    CHECK(compo.debug() == "Component");
}

TEST_CASE("Component get errors") {
    struct MyBasicCompo : public Component {
        int data;
        MyBasicCompo() {
            port("p1", &MyBasicCompo::data);
            port("p2", &MyBasicCompo::data);
        }
    };
    MyBasicCompo compo{};
    TINYCOMPO_TEST_ERRORS { compo.get("p3"); }
    TINYCOMPO_TEST_ERRORS_END("<Component::get> Port name p3 not found. Existing ports are:\n  * p1\n  * p2\n");
    TINYCOMPO_TEST_MORE_ERRORS { compo.get<int>("p3"); }
    TINYCOMPO_TEST_ERRORS_END("<Component::get<Interface>> Port name p3 not found. Existing ports are:\n  * p1\n  * p2\n");
}

/*
=============================================================================================================================
  ~*~ _ComponentBuilder ~*~
===========================================================================================================================*/
TEST_CASE("_ComponentBuilder tests.") {
    _ComponentBuilder compo(_Type<MyCompo>(), "youpi", 3, 4);  // create _ComponentBuilder object
    auto ptr = compo._constructor();                           // instantiate actual object
    auto ptr2 = dynamic_cast<MyCompo*>(ptr.get());
    REQUIRE(ptr2 != nullptr);
    CHECK(ptr2->i == 3);
    CHECK(ptr2->j == 4);
    CHECK(compo.name == "youpi");
    CHECK(compo.type == "MyCompo");  // technically compiler-dependant, but should work with gcc/clang
}

/*
=============================================================================================================================
  ~*~ Address ~*~
===========================================================================================================================*/
struct MyKey {
    int i;
};

ostream& operator<<(ostream& os, MyKey const& m) { return os << m.i; }

TEST_CASE("Address to stream") {
    std::stringstream ss;
    Address a("a", "b", "c");
    ss << a;
    CHECK(ss.str() == "a__b__c");
}

TEST_CASE("key_to_string test.") {
    CHECK(key_to_string(3) == "3");
    CHECK(key_to_string("yolo") == "yolo");
    MyKey key = {3};
    CHECK(key_to_string(key) == "3");
}

TEST_CASE("Address tests.") {
    auto a = Address("a", 2, 3, "b");
    CHECK(a.first() == "a");
    CHECK(a.rest().first() == "2");
    CHECK(a.rest().rest().first() == "3");
    CHECK(a.rest().rest().rest().first() == "b");

    CHECK(a.is_composite() == true);
    CHECK(Address("youpi").is_composite() == false);

    CHECK(a.to_string() == "a__2__3__b");
    CHECK(Address(a, 17).to_string() == "a__2__3__b__17");

    Address b("a", "b");
    Address c("c", "d");
    Address e(b, c);
    CHECK(e.to_string() == "a__b__c__d");
}

TEST_CASE("Address: builder from string") {
    Address a("Omega__3__1");
    CHECK(a.first() == "Omega");
    CHECK(a.rest().first() == "3");
    CHECK(a.rest().rest().first() == "1");
}

TEST_CASE("Address: ==  operator") {
    Address abc("a", "b", "c");
    Address abb("a", "b", "b");
    Address ab("a", "b");
    Address abc2("a", "b", "c");

    CHECK(not(abc == abb));
    CHECK(not(abc == ab));
    CHECK(abc == abc);
    CHECK(abc == abc2);
}

TEST_CASE("PortAddress == operator") {
    PortAddress ra("ptr", "a");
    PortAddress rab("ptr", "a", "b");
    PortAddress ta("ptt", "a");
    PortAddress rab2("ptr", "a", "b");

    CHECK(not(ra == rab));
    CHECK(not(ra == ta));
    CHECK(rab == rab);
    CHECK(rab == rab2);
}

TEST_CASE("Address: is_ancestor") {
    Address e;
    Address a("a");
    Address abc("a", "b", "c");
    Address abc2("a", "b", "c");
    Address abd("a", "b", "d");
    Address dbc("d", "b", "c");
    Address ab("a", "b");

    CHECK(e.is_ancestor(a));
    CHECK(e.is_ancestor(abc));
    CHECK(a.is_ancestor(abc));
    CHECK(abc.is_ancestor(abc));
    CHECK(abc.is_ancestor(abc2));
    CHECK(ab.is_ancestor(abc));
    CHECK(not ab.is_ancestor(dbc));
    CHECK(not abc.is_ancestor(abd));
}

TEST_CASE("Address: rebase") {
    Address ab("a", "b");
    Address abcd("a", "b", "c", "d");
    Address cd("c", "d");

    CHECK(abcd.rebase(ab) == cd);
    TINYCOMPO_TEST_ERRORS { cd.rebase(ab); }
    TINYCOMPO_TEST_ERRORS_END("Trying to rebase address c__d from a__b although it is not an ancestor!\n")
}

TEST_CASE("Address: error for keyrs with __") {
    TINYCOMPO_TEST_ERRORS { Address a("a", "b", "c__d"); }
    TINYCOMPO_TEST_ERRORS_END("Trying to add key c__d (which contains __) to address a__b\n");

    TINYCOMPO_TEST_MORE_ERRORS { Address a(Address("a", "b"), "c__d"); }
    TINYCOMPO_TEST_ERRORS_END("Trying to add key c__d (which contains __) to address a__b\n");
}

TEST_CASE("Address: avoiding transforming addresses to keys with __") {
    Address b("c", "d");
    Address a(Address("a", "b"), b);
}

/*
=============================================================================================================================
  ~*~ Model ~*~
===========================================================================================================================*/
TEST_CASE("model test: components in composites") {
    Model model;
    model.composite("compo0");
    model.component<MyInt>(Address("compo0", 1), 5);
    model.composite(Address("compo0", 2));
    model.component<MyInt>(Address("compo0", 2, 1), 3);

    CHECK(model.size() == 1);  // top level contains only one component which is a composite
    auto& compo0 = model.get_composite("compo0");
    CHECK(compo0.size() == 2);
    auto& compo0_2 = compo0.get_composite(2);
    CHECK(compo0_2.size() == 1);
    auto& compo0_3 = model.get_composite(Address("compo0", 2));
    CHECK(compo0_3.size() == 1);

    TINYCOMPO_TEST_ERRORS { model.component<MyInt>(Address("badAddress", 1), 2); }
    TINYCOMPO_TEST_ERRORS_END(
        "Composite not found. Composite badAddress does not exist. Existing composites are:\n  * compo0\n");
}

TEST_CASE("model test: model copy") {
    Model model;
    model.composite("compo0");
    auto model2 = model;
    model2.component<MyInt>(Address("compo0", 1), 19);
    model2.component<MyInt>("compo1", 17);
    CHECK(model.size() == 1);
    CHECK(model2.size() == 2);
}

TEST_CASE("model test: composite referencees") {
    Model model;
    model.composite("compo0");
    auto& compo0ref = model.get_composite("compo0");
    compo0ref.component<MyCompo>(1, 17, 18);
    compo0ref.component<MyCompo>(2, 21, 22);
    CHECK(model.size() == 1);
    CHECK(model.get_composite("compo0").size() == 2);
}

struct MyBasicCompo : public Component {
    MyBasicCompo* buddy{nullptr};
    std::string data;
    MyBasicCompo() {
        port("buddy", &MyBasicCompo::setBuddy);
        port("data", &MyBasicCompo::data);
    }
    void setBuddy(MyBasicCompo* buddyin) { buddy = buddyin; }
};

TEST_CASE("Model test: dot output and representation print") {
    Model model;
    model.component<MyBasicCompo>("mycompo");
    model.composite("composite");
    model.component<MyBasicCompo>(Address("composite", 2));
    model.connect<Use<MyBasicCompo>>(PortAddress("buddy", "mycompo"), Address("composite", 2));

    stringstream ss;
    model.dot(ss);
    CHECK(ss.str() ==
          "graph g {\n\tmycompo [label=\"mycompo\\n(MyBasicCompo)\" shape=component margin=0.15];\n\tconnect_0 "
          "[xlabel=\"tc::Use<MyBasicCompo>\" shape=point];\n\tconnect_0 -- mycompo[xlabel=\"buddy\"];\n\tconnect_0 -- "
          "composite__2;\n\tsubgraph cluster_composite {\n\t\tcomposite__2 [label=\"2\\n(MyBasicCompo)\" shape=component "
          "margin=0.15];\n\t}\n}\n");

    stringstream ss2;
    model.print(ss2);
    CHECK(
        ss2.str() ==
        "Component \"mycompo\" (MyBasicCompo)\nConnector (tc::Use<MyBasicCompo>) ->mycompo.buddy ->composite__2 \nComposite "
        "composite {\n	Component \"2\" (MyBasicCompo)\n}\n");
}

TEST_CASE("Model test: addresses passed as strings detected as such by representation") {
    Model model;
    model.component<MyBasicCompo>("mycompo")
        .connect<Use<MyBasicCompo>>("buddy", "compo2")
        .connect<Set<string>>("data", "youpi");

    stringstream ss;
    model.print(ss);
    CHECK((ss.str() ==
               "Component \"mycompo\" (MyBasicCompo)\n"
               "Connector (tc::Use<MyBasicCompo>) ->mycompo.buddy ->compo2 \n"
               "Connector (tc::Set<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > >) "
               "->mycompo.data \n" or
           ss.str() == "Component \"mycompo\" (MyBasicCompo)\n"
                       "Connector (tc::Use<MyBasicCompo>) ->mycompo.buddy ->compo2 \n"
                       "Connector (tc::Set<std::string>) ->mycompo.data \n"));
}

TEST_CASE("Model test: temporary keys") {
    Model model;
    for (int i = 0; i < 5; i++) {
        stringstream ss;
        ss << "compo" << i;
        model.component<MyInt>(ss.str());
    }
    CHECK(model.size() == 5);
}

TEST_CASE("Model test: copy") {
    Model model2;
    model2.component<MyIntProxy>("compo3");

    Assembly assembly(model2);

    Model model3 = model2;  // copy
    model3.component<MyInt>("youpi", 17);
    model2.component<MyInt>("youpla", 19);

    Assembly assembly3(model3);
    CHECK(assembly3.at<MyInt>("youpi").get() == 17);
    TINYCOMPO_TEST_ERRORS { assembly3.at("youpla"); }
    TINYCOMPO_THERE_WAS_AN_ERROR;

    Assembly assembly4(model2);
    CHECK(assembly4.at<MyInt>("youpla").get() == 19);
    TINYCOMPO_TEST_MORE_ERRORS { assembly4.at("youpi"); }
    TINYCOMPO_THERE_WAS_AN_ERROR;
    TINYCOMPO_TEST_MORE_ERRORS { assembly.at("youpi"); }  // checking that assembly (originally built from model2)
    TINYCOMPO_THERE_WAS_AN_ERROR;                         // has not been modified (and has an actual copy)
    TINYCOMPO_TEST_MORE_ERRORS { assembly.at("youpla"); }
    TINYCOMPO_THERE_WAS_AN_ERROR;
}

TEST_CASE("Model test: copy and composites") {
    Model model1;
    model1.composite("composite");
    Model model2 = model1;  // copy
    model1.component<MyInt>(Address("composite", 'r'), 17);
    stringstream ss;
    model2.print(ss);  // should not contain composite_r
    CHECK(ss.str() == "Composite composite {\n}\n");
}

TEST_CASE("Model test: digraph export") {
    Model model;
    model.component<MyInt>("d", 3);
    model.component<MyInt>("e", 5);
    model.component<IntReducer>("c");
    model.connect<Use<IntInterface>>(PortAddress("ptr", "c"), Address("d"));
    model.connect<Use<IntInterface>>(PortAddress("ptr", "c"), Address("e"));
    // model.connect<Use<IntInterface>>(PortAddress("ptr", "c"), "e"); // TODO TODO WHY DOES THIS WORKS
    model.component<MyIntProxy>("a");
    model.component<MyIntProxy>("b");
    model.connect<Use<IntInterface>>(PortAddress("ptr", "a"), Address("c"));
    model.connect<Use<IntInterface>>(PortAddress("ptr", "b"), Address("c"));

    auto graph = model.get_digraph();
    CHECK((graph.first == set<string>{"a", "b", "c", "d", "e"}));
    CHECK((graph.second ==
           multimap<string, string>{make_pair("a", "c"), make_pair("b", "c"), make_pair("c", "d"), make_pair("c", "e")}));
}

TEST_CASE("_AssemblyGraph test: all_component_names") {
    Model model;
    model.component<MyInt>(0, 17);
    model.component<MyInt>(2, 31);
    model.composite(1);
    model.component<MyInt>(Address(1, 'r'), 21);
    model.composite(Address(1, 't'));
    model.component<MyInt>(Address(1, 't', 'l'), 23);

    vector<string> vec0 = model.all_component_names();
    vector<string> vec1 = model.all_component_names(1);
    vector<string> vec2 = model.all_component_names(2);
    vector<string> vec3 = model.all_component_names(2, true);
    CHECK((set<string>(vec0.begin(), vec0.end())) == (set<string>{"0", "2"}));
    CHECK((set<string>(vec1.begin(), vec1.end())) == (set<string>{"0", "2", "1__r"}));
    CHECK((set<string>(vec2.begin(), vec2.end())) == (set<string>{"0", "2", "1__r", "1__t__l"}));
    CHECK((set<string>(vec3.begin(), vec3.end())) == (set<string>{"0", "1", "1__t", "2", "1__r", "1__t__l"}));
}

TEST_CASE("Model test: composite not found") {
    Model model;
    model.composite("youpi");
    model.composite("youpla");
    TINYCOMPO_TEST_ERRORS { model.get_composite("youplaboum"); }
    TINYCOMPO_TEST_ERRORS_END(
        "Composite not found. Composite youplaboum does not exist. Existing composites are:\n  * youpi\n  * youpla\n");
    const Model model2{model};  // testing const version of error
    TINYCOMPO_TEST_MORE_ERRORS { model2.get_composite("youplaboum"); }
    TINYCOMPO_TEST_ERRORS_END(
        "Composite not found. Composite youplaboum does not exist. Existing composites are:\n  * youpi\n  * youpla\n");
}

TEST_CASE("Model test: is_composite") {
    Model model;
    model.component<MyInt>("a", 17);
    model.composite("b");
    model.component<MyInt>(Address("b", "c"), 19);

    CHECK(model.is_composite("a") == false);
    CHECK(model.is_composite("b") == true);
    CHECK(model.is_composite(Address("b", "c")) == false);
}

TEST_CASE("Model test: has_type") {
    Model model;
    model.component<MyInt>("a", 17);
    model.composite("b");
    model.component<MyIntProxy>(Address("b", "c"));

    CHECK(model.has_type<MyInt>("a") == true);
    CHECK(model.has_type<MyIntProxy>("a") == false);
    CHECK(model.has_type<MyInt>("b") == false);
    CHECK(model.has_type<MyIntProxy>("b") == false);
    CHECK(model.has_type<MyInt>(Address("b", "c")) == false);
    CHECK(model.has_type<MyIntProxy>(Address("b", "c")) == true);
}

TEST_CASE("Model test: exists") {
    Model model;
    model.component<MyInt>("a", 17);
    model.composite("b");
    model.component<MyInt>(Address("b", "c"), 19);

    CHECK(model.exists("a") == true);
    CHECK(model.exists("c") == false);
    CHECK(model.exists("youplaboum") == false);
    CHECK(model.exists("b") == true);
    CHECK(model.exists(Address("b", "c")) == true);
}

TEST_CASE("Model test: all_addresses") {
    Model model;
    model.component<MyInt>("a", 17);
    model.composite("b");
    model.composite(Address("b", "c"));
    model.component<MyInt>(Address("b", "c", "d"), 19);

    vector<Address> expected_result{"a", Address("b", "c", "d")}, expected_result2{Address("c", "d")};
    CHECK(model.all_addresses() == expected_result);
    CHECK(model.all_addresses("b") == expected_result2);
}

/*
=============================================================================================================================
  ~*~ Meta things ~*~
===========================================================================================================================*/
template <class Interface>
struct UseOrArrayUse : public Meta {
    static void connect(Model& model, PortAddress user, Address provider) {
        if (model.is_composite(provider)) {
            model.connect<MultiUse<Interface>>(user, provider);
        } else {
            model.connect<Use<Interface>>(user, provider);
        }
    }
};

TEST_CASE("Meta connections") {
    Model model;
    model.component<Array<MyInt>>("array", 5, 17);
    model.component<IntReducer>("reducer");
    model.component<MyIntProxy>("proxy");
    model.connect<UseOrArrayUse<IntInterface>>(PortAddress("ptr", "reducer"), Address("array"));
    model.connect<UseOrArrayUse<IntInterface>>(PortAddress("ptr", "proxy"), Address("reducer"));

    Assembly assembly(model);
    CHECK(assembly.at<IntInterface>("proxy").get() == 170);
}

struct MyIntWrapper : Meta {
    static ComponentReference connect(Model& model, Address name, int value) { return model.component<MyInt>(name, value); }
};

TEST_CASE("Meta components") {
    Model model;
    model.composite("a");
    model.component<MyIntWrapper>(Address("a", "b"), 17);

    Assembly assembly(model);
    CHECK(assembly.at<IntInterface>(Address("a", "b")).get() == 17);
}

/*
=============================================================================================================================
  ~*~ Assembly ~*~
===========================================================================================================================*/
TEST_CASE("Assembly test: instances and call.") {
    Model a;
    a.component<MyCompo>("Compo1", 13, 14);
    a.component<MyCompo>("Compo2", 15, 16);
    CHECK(a.size() == 2);
    Assembly b(a);
    auto& ref = b.at<MyCompo&>("Compo1");
    auto& ref2 = b.at<MyCompo&>("Compo2");
    CHECK(ref.j == 14);
    CHECK(ref2.j == 16);
    b.call("Compo2", "myPort", 77, 79);
    CHECK(ref2.i == 77);
    CHECK(ref2.j == 79);
    b.call(PortAddress("myPort", "Compo2"), 17, 19);
    CHECK(ref2.i == 17);
    CHECK(ref2.j == 19);
    stringstream ss;
    b.print(ss);
    CHECK(ss.str() == "Compo1: MyCompo\nCompo2: MyCompo\n");
}

TEST_CASE("Assembly test: instantiating composites.") {
    Model model;
    model.composite("composite");
    model.component<MyInt>(Address("composite", 0), 12);
    Assembly assembly(model);

    stringstream ss;
    assembly.print(ss);
    CHECK(ss.str() == "composite: Composite {\n0: MyInt\n}\n");
    auto& refComposite = assembly.at<Assembly>("composite");
    CHECK(refComposite.size() == 1);
    CHECK(refComposite.at<MyInt>(0).get() == 12);
}

TEST_CASE("Assembly test: sub-addressing tests.") {
    Model model;
    model.composite("Array");
    model.component<MyCompo>(Address("Array", 0), 12, 13);
    model.component<MyCompo>(Address("Array", 1), 15, 19);
    model.composite(Address("Array", 2));
    model.component<MyCompo>(Address("Array", 2, "youpi"), 7, 9);
    Assembly assembly(model);

    auto& arrayRef = assembly.at<Assembly>("Array");
    CHECK(arrayRef.size() == 3);
    auto& subArrayRef = assembly.at<Assembly>(Address("Array", 2));
    CHECK(subArrayRef.size() == 1);
    auto& subRef = assembly.at<MyCompo>(Address("Array", 1));
    auto& subSubRef = assembly.at<MyCompo>(Address("Array", 2, "youpi"));
    CHECK(subRef.i == 15);
    CHECK(subSubRef.i == 7);
}

TEST_CASE("Assembly test: incorrect address.") {
    Model model;
    model.component<MyCompo>("compo0");
    model.component<MyCompo>("compo1");
    Assembly assembly(model);
    TINYCOMPO_TEST_ERRORS { assembly.at<MyCompo>("compo"); }
    TINYCOMPO_TEST_ERRORS_END(
        "<Assembly::at> Trying to access incorrect address. Address compo does not exist. Existing addresses are:\n  * "
        "compo0\n  * compo1\n");
}

TEST_CASE("Assembly test: component names.") {
    Model model;
    model.component<MyCompo>("compoYoupi");
    model.component<MyCompo>("compoYoupla");
    model.composite("composite");
    model.component<MyCompo>(Address("composite", 3));
    Assembly assembly(model);
    CHECK(assembly.at<MyCompo>("compoYoupi").get_name() == "compoYoupi");
    CHECK(assembly.at<MyCompo>("compoYoupla").get_name() == "compoYoupla");
    CHECK(assembly.at<MyCompo>(Address("composite", 3)).get_name() == "composite__3");
}

TEST_CASE("Assembly test: get_model.") {
    Model model;
    model.component<MyCompo>("youpi");
    Assembly assembly(model);
    Model model2 = assembly.get_model();
    model.component<MyCompo>("youpla");
    CHECK(model2.size() == 1);
    CHECK(model.size() == 2);
    stringstream ss;
    model2.print(ss);
    CHECK(ss.str() == "Component \"youpi\" (MyCompo)\n");  // technically compiler-dependant
}

TEST_CASE("Assembly test: composite ports.") {
    struct GetInt {
        virtual int getInt() = 0;
    };
    struct User : public Component {
        GetInt* ptr{nullptr};
        void setPtr(GetInt* ptrin) { ptr = ptrin; }
        User() { port("ptr", &User::setPtr); }
    };
    struct Two : public GetInt {
        int getInt() override { return 2; }
    };
    struct Provider : public Component {
        Two two;
        GetInt* providePtr() { return &two; }
        Provider() { provide("int", &Provider::providePtr); }
    };

    struct MyFancyComposite : public Composite {
        void after_construct() override {
            provide<IntInterface>("int", Address("a"));
            provide<IntInterface>("proxy", Address("b"));
            provide<GetInt>("prov", PortAddress("int", "p"));
        }

        static void contents(Model& model) {
            model.component<MyInt>("a", 7);
            model.component<MyIntProxy>("b");
            model.connect<Use<IntInterface>>(PortAddress("ptr", "b"), "a");
            model.component<Provider>("p");
        }
    };

    Model model;
    model.component<MyFancyComposite>("composite");
    model.component<MyIntProxy>("myProxy");
    model.connect<UseProvide<IntInterface>>(PortAddress("ptr", "myProxy"), PortAddress("int", "composite"));
    model.component<User>("u");
    model.connect<UseProvide<GetInt>>(PortAddress("ptr", "u"), PortAddress("prov", "composite"));

    Assembly assembly(model);
    CHECK(assembly.at<IntInterface>("myProxy").get() == 14);
    CHECK(assembly.at<User>("u").ptr->getInt() == 2);
}

TEST_CASE("Assembly: derives_from and is_composite") {
    Model model;
    model.component<MyInt>("a", 1);
    model.composite("b");
    model.component<MyInt>(Address("b", "c"), 3);
    model.composite(Address("b", "d"));

    Assembly assembly(model);
    CHECK(assembly.is_composite("a") == false);
    CHECK(assembly.is_composite("b") == true);
    CHECK(assembly.is_composite(Address("b", "c")) == false);
    CHECK(assembly.is_composite(Address("b", "d")) == true);
    CHECK(assembly.derives_from<IntInterface>("a") == true);
    CHECK(assembly.derives_from<IntInterface>("b") == false);
}

TEST_CASE("Assembly: instantiate from new model") {
    Model model;
    model.component<MyInt>("a", 1);
    model.composite("b");
    model.component<MyInt>(Address("b", "c"), 3);
    Model model2;
    model2.component<MyInt>("a", 3);
    model2.composite("c");
    model2.component<MyInt>(Address("c", "d"), 17);

    Assembly assembly(model);
    assembly.instantiate_from(model2);
    CHECK(assembly.at<MyInt>("a").get() == 3);
    CHECK(assembly.at<MyInt>(Address("c", "d")).get() == 17);
    TINYCOMPO_TEST_ERRORS { assembly.at<MyInt>(Address("b", "c")); }
    TINYCOMPO_TEST_ERRORS_END(
        "<Assembly::at> Trying to access incorrect address. Address b does not exist. "
        "Existing addresses are:\n  * a\n  * c\n");
}

TEST_CASE("Assembly: at with port address") {
    class SillyWrapper : public Component {
        MyInt wrappee;
        MyInt* provide_wrappee() { return &wrappee; }

      public:
        SillyWrapper(int init) : wrappee(init) { provide("port", &SillyWrapper::provide_wrappee); }
    };

    Model model;
    model.component<SillyWrapper>("c", 1717);

    Assembly assembly(model);
    auto& wref = assembly.at<MyInt>(PortAddress("port", "c"));
    CHECK(wref.get() == 1717);
}

TEST_CASE("Assembly: at with port address with composite port") {
    struct SillyWrapper : public Composite {
        void after_construct() override { provide<MyInt>("port", "c"); }
        static void contents(Model& m, int i) { m.component<MyInt>("c", i); }
    };

    Model model;
    model.component<SillyWrapper>("c", 1717);

    Assembly assembly(model);
    auto& wref = assembly.at<MyInt>(PortAddress("port", "c"));
    CHECK(wref.get() == 1717);
}

TEST_CASE("Assembly: get_all") {
    Model m;
    m.component<MyInt>("c0", 21);
    m.component<MyInt>("c1", 11);
    m.composite("box");
    m.component<MyInt>(Address("box", "c0"), 13);
    m.component<MyInt>(Address("box", "c1"), 17);
    m.component<MyIntProxy>("c3").connect<Use<IntInterface>>("ptr", Address("box", "c0"));

    Assembly a(m);
    auto all_myint = a.get_all<MyInt>();
    auto all_intinterface = a.get_all<IntInterface>();
    CHECK(accumulate(all_myint.pointers().begin(), all_myint.pointers().end(), 0,
                     [](int acc, MyInt* ptr) { return acc + ptr->i; }) == 62);
    CHECK(accumulate(all_intinterface.pointers().begin(), all_intinterface.pointers().end(), 0,
                     [](int acc, IntInterface* ptr) { return acc + ptr->get(); }) == 88);
}

TEST_CASE("Assembly: get_all in composites") {
    Model m;
    m.component<MyInt>("c0", 13);
    m.composite("a");
    m.component<MyInt>(Address("a", "c1"), 15);
    m.composite("b");
    m.component<MyInt>(Address("b", "c2"), 17);
    m.composite("c");
    m.component<MyInt>(Address("c", "c3"), 19);
    m.component<MyInt>(Address("c", "c4"), 31);

    Assembly a(m);
    std::vector<Address> expected1{Address("a", "c1"), Address("c", "c3"), Address("c", "c4")}, expected2{"c2"},
        expected3{"c3", "c4"};
    auto result1 = a.get_all<MyInt>(std::set<Address>{"a", "c"}).names();
    auto result2 = a.get_all<MyInt>("b").names();
    auto result3 = a.get_all<MyInt>(std::set<Address>{"c"}, "c").names();
    CHECK(result1 == expected1);
    CHECK(result2 == expected2);
    CHECK(result3 == expected3);
}

/*
=============================================================================================================================
  ~*~ Composite ~*~
===========================================================================================================================*/
TEST_CASE("Instantiation of lone composite") {
    struct MyComposite : public Composite {
        static void contents(Model& m, int i) {
            m.component<MyInt>("compo1", i);
            m.component<MyIntProxy>("compo2").connect<Use<IntInterface>>("ptr", "compo1");
        }

        void after_construct() override { provide<IntInterface>("interface", "compo2"); }
    };

    MyComposite c;
    instantiate_composite(c, 17);
    auto value = c.get<IntInterface>("interface")->get();
    CHECK(value == 34);
}

/*
=============================================================================================================================
  ~*~ ComponentReference ~*~
===========================================================================================================================*/
TEST_CASE("ComponentReference test") {
    Model model;
    auto a = model.component<MyInt>("a", 7);
    auto b = model.component<MyIntProxy>("b");
    b.connect<Use<IntInterface>>("ptr", a);

    auto c = model.composite("c");
    auto d = model.component<MyInt>(Address("c", "d"), 8);
    auto e = model.component<MyIntProxy>(Address("c", "e")).connect<Use<IntInterface>>("ptr", d);

    Assembly assembly(model);
    CHECK(assembly.at<IntInterface>("b").get() == 14);
    CHECK(assembly.at<IntInterface>(Address("c", "e")).get() == 16);
}

TEST_CASE("ComponentReference set test") {
    Model model;
    model.component<MyCompo>("compo").set("myPort", 19, 77);

    Assembly assembly(model);
    CHECK(assembly.at<MyCompo>("compo").i == 19);
    CHECK(assembly.at<MyCompo>("compo").j == 77);
}

/*
=============================================================================================================================
  ~*~ Configure ~*~
===========================================================================================================================*/
TEST_CASE("Configure test") {
    Model model;
    model.component<MyInt>("Compo1", 4);
    model.configure("Compo1", [](MyInt& r) { r.set(17); });

    Assembly assembly(model);
    CHECK(assembly.at<MyInt>("Compo1").get() == 17);
}

TEST_CASE("Configure with component references") {
    Model model;
    model.component<MyInt>("Compo1", 4).configure([](MyInt& r) { r.set(17); });

    Assembly assembly(model);
    CHECK(assembly.at<MyInt>("Compo1").get() == 17);
}

/*
=============================================================================================================================
  ~*~ Ports ~*~
===========================================================================================================================*/
TEST_CASE("Use/provide test.") {
    Model model;
    model.component<MyInt>("Compo1", 4);
    model.component<MyIntProxy>("Compo2");
    Assembly assembly(model);
    stringstream ss;
    assembly.print(ss);
    CHECK(ss.str() == "Compo1: MyInt\nCompo2: MyIntProxy\n");
    Use<IntInterface>::_connect(assembly, PortAddress("ptr", "Compo2"), Address("Compo1"));
    CHECK(assembly.at<MyIntProxy>("Compo2").get() == 8);
}

TEST_CASE("Use + Assembly: connection test") {
    Model model;
    model.component<MyInt>("Compo1", 4);
    model.component<MyIntProxy>("Compo2");
    model.connect<Use<IntInterface>>(PortAddress("ptr", "Compo2"), Address("Compo1"));
    Assembly assembly(model);
    CHECK(assembly.at<MyIntProxy>("Compo2").get() == 8);
}

TEST_CASE("UseProvide test.") {
    struct GetInt {
        virtual int getInt() = 0;
    };
    struct User : public Component {
        GetInt* ptr{nullptr};
        void setPtr(GetInt* ptrin) { ptr = ptrin; }
        User() { port("ptr", &User::setPtr); }
    };
    struct Two : public GetInt {
        int getInt() override { return 2; }
    };
    struct Provider : public Component {
        Two two;
        GetInt* providePtr() { return &two; }
        Provider() { provide("int", &Provider::providePtr); }
    };
    Model model;
    model.component<User>("user");
    model.component<Provider>("provider");
    model.connect<UseProvide<GetInt>>(PortAddress("ptr", "user"), PortAddress("int", "provider"));

    Assembly assembly(model);
    CHECK(assembly.at<User>("user").ptr->getInt() == 2);
}

TEST_CASE("Set test") {
    Model model;
    model.component<MyCompo>("compo", 2, 3);
    model.connect<Set<int, int>>(PortAddress("myPort", "compo"), 5, 7);
    Assembly assembly(model);
    CHECK(assembly.at<MyCompo>("compo").i == 5);
    CHECK(assembly.at<MyCompo>("compo").j == 7);
}

TEST_CASE("Attribute port declaration.") {
    struct MyUltraBasicCompo : public Component {
        int data{0};
        MyUltraBasicCompo() { port("data", &MyUltraBasicCompo::data); }
    };
    Model model;
    model.component<MyUltraBasicCompo>("compo");
    model.connect<Set<int>>(PortAddress("data", "compo"), 14);
    Assembly assembly(model);
    CHECK(assembly.at<MyUltraBasicCompo>("compo").data == 14);
}

/*
=============================================================================================================================
  ~*~ Drivers ~*~
===========================================================================================================================*/
TEST_CASE("Basic driver test.") {
    struct MyWrapper : public Component {
        MyInt state;
        MyInt* provideState() { return &state; }
        MyWrapper() { provide("state", &MyWrapper::provideState); }
    };

    Model model;
    model.component<MyInt>("c1", 119);
    model.component<MyWrapper>("c2");
    model.driver("driver", [](MyInt* r, MyInt* r2) {
        r->set(111);
        r2->set(1111);
    });
    model.connect<DriverConnect<Address, PortAddress>>("driver", "c1", PortAddress("state", "c2"));

    Assembly assembly(model);
    assembly.call("driver", "go");
    CHECK(assembly.at<MyInt>("c1").get() == 111);
    CHECK(assembly.at<MyWrapper>("c2").state.get() == 1111);
}

TEST_CASE("Driver connect short syntax") {
    Model model;
    model.component<MyInt>("c1", 19);
    model.component<MyInt>("c2", 321);
    model
        .driver("driver",
                [](MyInt* p1, MyInt* p2) {
                    p1->set(17);
                    p2->set(37);
                })
        .connect("c1", "c2");

    Assembly assembly(model);
    assembly.call("driver", "go");
    CHECK(assembly.at<MyInt>("c1").get() == 17);
    CHECK(assembly.at<MyInt>("c2").get() == 37);
}

/*
=============================================================================================================================
  ~*~ Component sets ~*~
===========================================================================================================================*/
TEST_CASE("Basic InstanceSet test.") {
    InstanceSet<MyInt> cs, cs2;
    MyInt a(13);
    MyInt b(17);
    MyInt c(19);
    cs2.push_back("a", &a);
    cs.combine(cs2);
    cs.push_back(Address("b"), &b);
    cs.push_back(Address("composite", "c"), &c);
    vector<string> observed_names, expected_names{"a", "b", "composite__c"};
    vector<int> observed_ints, expected_ints{13, 17, 19};
    for (auto&& name : cs.names()) {
        observed_names.push_back(name.to_string());
    }
    for (auto&& ptr : cs.pointers()) {
        observed_ints.push_back(ptr->get());
    }
    CHECK(observed_names == expected_names);
    CHECK(observed_ints == expected_ints);
}

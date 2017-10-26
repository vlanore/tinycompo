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
TEST_CASE("Exception tests") {
    TINYCOMPO_TEST_ERRORS {
        TinycompoDebug e{"my error"};
        e << "Something failed.";
        e.fail();
    }
    TINYCOMPO_TEST_ERRORS_END("my error", "-- Error: my error. Something failed.\n");
    CHECK(demangle("PFvPFvvEE") == "void (*)(void (*)())");
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
    CHECK(compo._debug() == "MyCompo");
    compo.set("myPort", 17, 18);
    CHECK(compo.i == 17);
    CHECK(compo.j == 18);
    TINYCOMPO_TEST_ERRORS { compo.set("myPort", true); }
    TINYCOMPO_TEST_ERRORS_END("setting property failed",
                              "-- Error: setting property failed. Type _Port<bool const> does not "
                              "seem to match port myPort.\n");
    TINYCOMPO_TEST_MORE_ERRORS { compo.set("badPort", 1, 2); }
    TINYCOMPO_TEST_ERRORS_END("port name not found",
                              "-- Error: port name not found. Could not find port badPort in component MyCompo.\n");
}

TEST_CASE("Component without _debug") {
    struct MyBasicCompo : public Component {};
    MyBasicCompo compo{};
    CHECK(compo._debug() == "Component");
}

/*
=============================================================================================================================
  ~*~ _ComponentBuilder ~*~
===========================================================================================================================*/
TEST_CASE("_ComponentBuilder tests.") {
    _ComponentBuilder compo(_Type<MyCompo>(), 3, 4);  // create _ComponentBuilder object
    auto ptr = compo._constructor();                  // instantiate actual object
    auto ptr2 = dynamic_cast<MyCompo*>(ptr.get());
    REQUIRE(ptr2 != nullptr);
    CHECK(ptr2->i == 3);
    CHECK(ptr2->j == 4);
    CHECK(ptr->_debug() == "MyCompo");
    CHECK(compo._class_name == "MyCompo");  // technically compiler-dependant, but should work with gcc/clang
}

/*
=============================================================================================================================
  ~*~ Address ~*~
===========================================================================================================================*/
struct MyKey {
    int i;
};

ostream& operator<<(ostream& os, MyKey const& m) { return os << m.i; }

TEST_CASE("key_to_string test.") {
    CHECK(key_to_string(3) == "3");
    CHECK(key_to_string("yolo") == "yolo");
    MyKey key = {3};
    CHECK(key_to_string(key) == "3");
}

TEST_CASE("address tests.") {
    auto a = Address("a", 2, 3, "b");
    CHECK(a.first() == "a");
    CHECK(a.rest().first() == "2");
    CHECK(a.rest().rest().first() == "3");
    CHECK(a.rest().rest().rest().first() == "b");

    CHECK(a.is_composite() == true);
    CHECK(Address("youpi").is_composite() == false);

    CHECK(a.to_string() == "a_2_3_b");
    CHECK(Address(a, 17).to_string() == "a_2_3_b_17");
}

TEST_CASE("Address: builder from string") {
    Address a = address_from_composite_string("Omega_3_1");
    CHECK(a.first() == "Omega");
    CHECK(a.rest().first() == "3");
    CHECK(a.rest().rest().first() == "1");
}

/*
=============================================================================================================================
  ~*~ Graph representation ~*~
===========================================================================================================================*/
TEST_CASE("_AssemblyGraph test: all_component_names") {
    Model model;
    model.component<MyInt>(0, 17);
    model.component<MyInt>(2, 31);
    model.composite(1);
    model.component<MyInt>(Address(1, 'r'), 21);
    model.composite(Address(1, 't'));
    model.component<MyInt>(Address(1, 't', 'l'), 23);

    auto& representation = dynamic_cast<const _AssemblyGraph&>(model.get_representation());

    vector<string> vec0 = representation.all_component_names();
    vector<string> vec1 = representation.all_component_names(1);
    vector<string> vec2 = representation.all_component_names(2);
    CHECK((set<string>(vec0.begin(), vec0.end())) == (set<string>{"0", "2"}));
    CHECK((set<string>(vec1.begin(), vec1.end())) == (set<string>{"0", "2", "1_r"}));
    CHECK((set<string>(vec2.begin(), vec2.end())) == (set<string>{"0", "2", "1_r", "1_t_l"}));
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
    // TINYCOMPO_TEST_ERRORS { model.component<MyInt>(Address("badAddress", 1), 2); }
    // TINYCOMPO_TEST_ERRORS_END("composite does not exist",
    //                           "-- Error: composite does not exist. Assembly contains no composite "
    //                           "at address badAddress.\n");
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

// TEST_CASE("model test: composite/component inheritance mismatch") {
//     class MyKey : public Composite {};
//     Model model;
//     TINYCOMPO_TEST_ERRORS { model.component<MyKey>("hello"); }
//     TINYCOMPO_TEST_ERRORS_END("trying to declare a component that does not inherit from Component",
//                               "-- Error: trying to declare a component that does not inherit from Component\n");
// }

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
    MyBasicCompo() { port("buddy", &MyBasicCompo::setBuddy); }
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
          "[xlabel=\"Use<MyBasicCompo>\" shape=point];\n\tconnect_0 -- mycompo[xlabel=\"buddy\"];\n\tconnect_0 -- "
          "composite_2;\n\tsubgraph cluster_composite {\n\t\tcomposite_2 [label=\"2\\n(MyBasicCompo)\" shape=component "
          "margin=0.15];\n\t}\n}\n");

    stringstream ss2;
    model.print_representation(ss2);
    CHECK(ss2.str() ==
          "Component \"mycompo\" (MyBasicCompo) \nConnector (Use<MyBasicCompo>) ->mycompo.buddy ->composite_2 \nComposite "
          "composite {\n	Component \"2\" (MyBasicCompo) \n}\n");
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

TEST_CASE("Model test: representation copy and composites") {
    Model model1;
    model1.composite("composite");
    Model model2 = model1;  // copy
    model1.component<MyInt>(Address("composite", 'r'), 17);
    stringstream ss;
    model2.print_representation(ss);  // should not contain composite_r
    CHECK(ss.str() == "Composite composite {\n}\n");
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
    stringstream ss;
    b.print_all(ss);
    CHECK(ss.str() == "Compo1: MyCompo\nCompo2: MyCompo\n");
}

TEST_CASE("Assembly test: instantiating composites.") {
    Model model;
    model.composite("composite");
    model.component<MyInt>(Address("composite", 0), 12);
    Assembly assembly(model);

    stringstream ss;
    assembly.print_all(ss);
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
    TINYCOMPO_TEST_ERRORS_END("<Assembly::at> Trying to access incorrect address",
                              "-- Error: <Assembly::at> Trying to access incorrect address. Address compo does not exist. "
                              "Existing addresses are:\n  * compo0\n  * compo1\n\n");
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
    CHECK(assembly.at<MyCompo>(Address("composite", 3)).get_name() == "composite_3");
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
    model2.print_representation(ss);
    CHECK(ss.str() == "Component \"youpi\" (MyCompo) \n");  // technically compiler-dependant
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
    assembly.print_all(ss);
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
    model.connect<Set>(PortAddress("myPort", "compo"), 5, 7);
    Assembly assembly(model);
    CHECK(assembly.at<MyCompo>("compo").i == 5);
    CHECK(assembly.at<MyCompo>("compo").j == 7);
}

TEST_CASE("ListUse test") {
    Model model;
    model.component<IntReducer>("user");
    model.composite("array");
    model.component<MyInt>(Address("array", 0), 1);
    model.component<MyInt>(Address("array", 1), 4);
    model.component<MyInt>(Address("array", 2), 12);
    model.component<MyInt>(Address("array", 3), 7);
    model.connect<ListUse<IntInterface>>(PortAddress("ptr", "user"), Address("array", 0), Address("array", 1),
                                         Address("array", 3));
    Assembly assembly(model);
    CHECK(assembly.at<IntInterface>("user").get() == 12);
}

TEST_CASE("Attribute port declaration.") {
    struct MyUltraBasicCompo : public Component {
        int data{0};
        MyUltraBasicCompo() { port("data", &MyUltraBasicCompo::data); }
    };
    Model model;
    model.component<MyUltraBasicCompo>("compo");
    model.connect<Set>(PortAddress("data", "compo"), 14);
    Assembly assembly(model);
    CHECK(assembly.at<MyUltraBasicCompo>("compo").data == 14);
}

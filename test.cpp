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

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"

#include "tinycompo.hpp"

/*
============================================= DEBUG ==============================================*/
#define TINYCOMPO_TEST_ERRORS                  \
    std::stringstream error_short, error_long; \
    TinycompoDebug::set_stream(error_long);    \
    try
#define TINYCOMPO_TEST_ERRORS_END    \
    catch (TinycompoException & e) { \
        error_short << e.what();     \
    }                                \
    TinycompoDebug::set_stream(std::cerr);

TEST_CASE("Exception tests") {
    TINYCOMPO_TEST_ERRORS {
        TinycompoDebug e{"my error"};
        e << "Something failed.";
        e.fail();
    }
    TINYCOMPO_TEST_ERRORS_END
    CHECK(error_short.str() == "my error");
    CHECK(error_long.str() == "-- Error: my error. Something failed.");
    CHECK(demangle("PFvPFvvEE") == "void (*)(void (*)())");
}

/*
============================================= _PORT ==============================================*/
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
=========================================== COMPONENT ============================================*/
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

TEST_CASE("Component tests.") {
    MyCompo compo{};
    // MyCompo compo2 = compo; // does not work because Component copy is forbidden (intentional)
    CHECK(compo._debug() == "MyCompo");
    compo.set("myPort", 17, 18);
    CHECK(compo.i == 17);
    CHECK(compo.j == 18);

    TINYCOMPO_TEST_ERRORS {
        compo.set("myPort", true);  // intentional error
    }
    TINYCOMPO_TEST_ERRORS_END
    CHECK(error_short.str() == "Setting property failed");
    CHECK(error_long.str() ==
          "-- Error: Setting property failed. Type _Port<bool const> does not seem to match port "
          "myPort.\n");
}

/*
=========================================== _COMPONENT ===========================================*/
TEST_CASE("_Component tests.") {
    _Component compo(_Type<MyCompo>(), 3, 4);  // create _Component object
    auto ptr = compo._constructor();           // instantiate actual object
    auto ptr2 = dynamic_cast<MyCompo*>(ptr.get());
    REQUIRE(ptr2 != nullptr);
    CHECK(ptr2->i == 3);
    CHECK(ptr2->j == 4);
    CHECK(ptr->_debug() == "MyCompo");
}

/*
=========================================== _OPERATION ===========================================*/
TEST_CASE("_Operation tests.") {
    class MyAssembly {
      public:
        MyCompo compo1{14, 15};
        MyCompo compo2{18, 19};
        Component& at(int) { return compo1; }
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

/*
============================================ ASSEMBLY ============================================*/
TEST_CASE("Assembly tests.") {
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
    TINYCOMPO_TEST_ERRORS {
        a.at("Compo1").set("myPort", 3, 3);  // triggering uninstantiated exception
    }
    TINYCOMPO_TEST_ERRORS_END
    CHECK(error_short.str() == "uninstantiated assembly");
    CHECK(error_long.str() ==
          "-- Error: uninstantiated assembly. Trying to call method at (direct) although the "
          "assembly is not instantiated!");
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

/*
========================================== USE/PROVIDE ===========================================*/
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

/*
============================================= ARRAY ==============================================*/
TEST_CASE("Array tests.") {
    Array<MyCompo> myArray(3, 11, 12);
    CHECK(myArray._debug() == "Array [\n0: MyCompo\n1: MyCompo\n2: MyCompo\n]");
    CHECK(myArray.size() == 3);
    auto& ref0 = myArray.at<MyCompo>(0);
    auto& ref2 = myArray.at<MyCompo>(2);
    ref2.i = 17;          // random number
    CHECK(ref0.i == 11);  // cf myArray init above
    CHECK(ref2.i == 17);
    auto& ref2bis = myArray.at<MyCompo>(2);
    CHECK(ref2bis.i == 17);
}

/*
========================================= ARRAYONETOONE ==========================================*/
TEST_CASE("Array connector tests.") {
    Assembly<> model;
    model.component<Array<MyInt>>("intArray", 5, 12);
    model.component<Array<MyIntProxy>>("proxyArray", 5);
    model.instantiate();
    ArrayOneToOne<IntInterface>::_connect(model, "proxyArray", "ptr", "intArray");
    CHECK(model.at<Assembly<int>>("intArray").size() == 5);
    auto& refElement1 = model.at<MyInt>("intArray", 1);
    CHECK(refElement1.get() == 12);
    refElement1.i = 23;
    CHECK(refElement1.get() == 23);
    CHECK(model.at<Assembly<int>>("proxyArray").size() == 5);
    CHECK(model.at<MyIntProxy>("proxyArray", 1).get() == 46);
    CHECK(model.at<MyIntProxy>("proxyArray", 4).get() == 24);
}

TEST_CASE("Array connector error test.") {
    Assembly<> model;
    model.component<Array<MyInt>>("intArray", 5, 12);
    model.component<Array<MyIntProxy>>("proxyArray", 4);  // intentionally mismatched arrays
    model.instantiate();
    TINYCOMPO_TEST_ERRORS {
        ArrayOneToOne<IntInterface>::_connect(model, "proxyArray", "ptr", "intArray");
    }
    TINYCOMPO_TEST_ERRORS_END
    CHECK(error_short.str() == "Array connection: mismatched sizes");
    CHECK(error_long.str() ==
          "-- Error: Array connection: mismatched sizes. proxyArray has size 4 while intArray has "
          "size 5.\n");
    TinycompoDebug::set_stream(std::cerr);
}

/*
============================================ MULTIUSE ============================================*/
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

TEST_CASE("MultiUse tests.") {
    Assembly<> model;
    model.component<Array<MyInt>>("intArray", 3, 12);
    model.component<IntReducer>("Reducer");
    model.instantiate();
    std::stringstream ss;
    model.print_all(ss);
    CHECK(ss.str() ==
          "Reducer: IntReducer\nintArray: Array [\n0: MyInt\n1: MyInt\n2: "
          "MyInt\n]\n");
    MultiUse<IntInterface>::_connect(model, "Reducer", "ptrs", "intArray");
    auto& refElement1 = model.at<MyInt>("intArray", 1);
    CHECK(refElement1.get() == 12);
    refElement1.i = 23;
    CHECK(refElement1.get() == 23);
    auto& refReducer = model.at<IntInterface>("Reducer");
    CHECK(refReducer.get() == 47);
}

/*
========================================== MULTIPROVIDE ==========================================*/
TEST_CASE("MultiProvide connector tests.") {
    Assembly<> model;
    model.component<MyInt>("superInt", 17);  // random number
    model.component<Array<MyIntProxy>>("proxyArray", 5);
    model.instantiate();
    MultiProvide<IntInterface>::_connect(model, "proxyArray", "ptr", "superInt");
    CHECK(model.at<MyIntProxy>("proxyArray", 2).get() == 34);
}

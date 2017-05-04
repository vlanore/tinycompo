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

#ifndef ARRAYS_HPP
#define ARRAYS_HPP

#include "tinycompo.hpp"

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
    explicit Array(int nbElems, Args... args) {
        for (int i = 0; i < nbElems; i++) {
            component<T>(i, std::forward<Args>(args)...);
        }
        instantiate();
    }
};

/*
============================================== TEST ==============================================*/
#ifdef DOCTEST_LIBRARY_INCLUDED
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
#endif  // DOCTEST_LIBRARY_INCLUDED

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
============================================== TEST ==============================================*/
#ifdef DOCTEST_LIBRARY_INCLUDED
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
    std::stringstream my_cerr{};
    TinycompoDebug::set_stream(my_cerr);
    std::stringstream error{};
    try {
        ArrayOneToOne<IntInterface>::_connect(model, "proxyArray", "ptr", "intArray");
    } catch (const TinycompoException& e) {
        error << e.what();
    }
    CHECK(error.str() == "Array connection: mismatched sizes");
    CHECK(my_cerr.str() ==
          "-- Error: Array connection: mismatched sizes. proxyArray has size 4 while intArray has "
          "size 5.\n");
    TinycompoDebug::set_stream(std::cerr);
}
#endif  // DOCTEST_LIBRARY_INCLUDED

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
============================================== TEST ==============================================*/
#ifdef DOCTEST_LIBRARY_INCLUDED
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

TEST_CASE("Reducer tests.") {
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
#endif  // DOCTEST_LIBRARY_INCLUDED

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
============================================== TEST ==============================================*/
#ifdef DOCTEST_LIBRARY_INCLUDED
TEST_CASE("MultiProvide connector tests") {
    Assembly<> model;
    model.component<MyInt>("superInt", 17);  // random number
    model.component<Array<MyIntProxy>>("proxyArray", 5);
    model.instantiate();
    MultiProvide<IntInterface>::_connect(model, "proxyArray", "ptr", "superInt");
    CHECK(model.at<MyIntProxy>("proxyArray", 2).get() == 34);
}
#endif  // DOCTEST_LIBRARY_INCLUDED

#endif  // ARRAYS_HPP

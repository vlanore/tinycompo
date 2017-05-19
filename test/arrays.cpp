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

#include "test_utils.hpp"

/*
====================================================================================================
  ~*~ Array ~*~
==================================================================================================*/
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
====================================================================================================
~*~ ArrayOneToOne ~*~
==================================================================================================*/
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
====================================================================================================
  ~*~ Multiuse ~*~
==================================================================================================*/
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
====================================================================================================
  ~*~ Multiprovide ~*~
==================================================================================================*/
TEST_CASE("MultiProvide connector tests.") {
    Assembly<> model;
    model.component<MyInt>("superInt", 17);  // random number
    model.component<Array<MyIntProxy>>("proxyArray", 5);
    model.instantiate();
    MultiProvide<IntInterface>::_connect(model, "proxyArray", "ptr", "superInt");
    CHECK(model.at<MyIntProxy>("proxyArray", 2).get() == 34);
}

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
    CHECK(myArray.size() == 3);
    Assembly<int> array(myArray);
    array.at<MyCompo>(2).i = 17;
    CHECK(array.at<MyCompo>(2).i == 17);
    CHECK(array.at<MyCompo>(0).i == 11);
}

/*
====================================================================================================
~*~ ArrayOneToOne ~*~
==================================================================================================*/
TEST_CASE("Array connector tests.") {
    Model<> model;
    model.composite<Array<MyInt>>("intArray", 5, 12);
    model.composite<Array<MyIntProxy>>("proxyArray", 5);
    Assembly<> assembly(model);
    ArrayOneToOne<IntInterface>::_connect(assembly, "proxyArray", "ptr", "intArray");
    CHECK(assembly.at<Assembly<int>>("intArray").size() == 5);
    auto& refElement1 = assembly.at<MyInt>(Address("intArray", 1));
    CHECK(refElement1.get() == 12);
    refElement1.i = 23;
    CHECK(refElement1.get() == 23);
    CHECK(assembly.at<Assembly<int>>("proxyArray").size() == 5);
    CHECK(assembly.at<MyIntProxy>(Address("proxyArray", 1)).get() == 46);
    CHECK(assembly.at<MyIntProxy>(Address("proxyArray", 4)).get() == 24);
}

TEST_CASE("Array connector error test.") {
    Model<> model;
    model.composite<Array<MyInt>>("intArray", 5, 12);
    model.composite<Array<MyIntProxy>>("proxyArray", 4);  // intentionally mismatched arrays
    Assembly<> assembly(model);
    TINYCOMPO_TEST_ERRORS {
        ArrayOneToOne<IntInterface>::_connect(assembly, "proxyArray", "ptr", "intArray");
    }
    TINYCOMPO_TEST_ERRORS_END("Array connection: mismatched sizes",
                              "-- Error: Array connection: mismatched sizes. proxyArray has size 4 "
                              "while intArray has size 5.\n");
}

/*
====================================================================================================
  ~*~ Multiuse ~*~
==================================================================================================*/
TEST_CASE("MultiUse tests.") {
    Model<> model;
    model.composite<Array<MyInt>>("intArray", 3, 12);
    model.component<IntReducer>("reducer");
    Assembly<> assembly(model);
    std::stringstream ss;
    assembly.print_all(ss);
    CHECK(ss.str() ==
          ""
          "intArray: Composite {\n0: MyInt\n1: MyInt\n2: MyInt\n}\nreducer: IntReducer\n");
    MultiUse<IntInterface>::_connect(assembly, "reducer", "ptr", "intArray");
    auto& refElement1 = assembly.at<MyInt>(Address("intArray", 1));
    CHECK(refElement1.get() == 12);
    refElement1.i = 23;
    CHECK(refElement1.get() == 23);
    auto& refReducer = assembly.at<IntInterface>("reducer");
    CHECK(refReducer.get() == 47);
}

/*
====================================================================================================
  ~*~ Multiprovide ~*~
==================================================================================================*/
TEST_CASE("MultiProvide connector tests.") {
    Model<> model;
    model.component<MyInt>("superInt", 17);  // random number
    model.composite<Array<MyIntProxy>>("proxyArray", 5);
    Assembly<> assembly(model);
    MultiProvide<IntInterface>::_connect(assembly, "proxyArray", "ptr", "superInt");
    CHECK(assembly.at<MyIntProxy>(Address("proxyArray", 2)).get() == 34);
}

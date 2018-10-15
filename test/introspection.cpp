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

#include "test_utils.hpp"
using namespace std;

/*
=============================================================================================================================
  ~*~ Size ~*~
===========================================================================================================================*/
TEST_CASE("Basic introspector size test") {
    Model m;
    m.component<MyIntProxy>("a");
    m.composite("b");
    auto& m2 = m.get_composite("b");
    m2.component<MyIntProxy>("c");
    m2.component<MyInt>("d", 14);
    m.connect<Use<IntInterface>>(PortAddress("ptr", "a"), Address("b", "c"));
    m.connect<Set<int>>(PortAddress("set", "b", "d"), 19);
    m2.connect<Use<IntInterface>>(PortAddress("ptr", "c"), "d");

    Introspector i(m);
    CHECK(i.nb_components() == 2);
    CHECK(i.nb_operations() == 2);
    CHECK(i.deep_nb_components() == 3);
    CHECK(i.deep_nb_operations() == 3);
}

/*
=============================================================================================================================
  ~*~ Topology ~*~
===========================================================================================================================*/
TEST_CASE("Introspector non-deep component test") {
    Model m;
    m.component<MyIntProxy>("a");
    m.composite("b");
    auto& m2 = m.get_composite("b");
    m2.component<MyIntProxy>("c");
    m2.component<MyInt>("d", 14);
    m.connect<Use<IntInterface>>(PortAddress("ptr", "a"), Address("b", "c"));
    m.connect<Set<int>>(PortAddress("set", "b", "d"), 19);
    m2.connect<Use<IntInterface>>(PortAddress("ptr", "c"), "d");

    Introspector i(m);
    std::vector<Address> expected_components{"a", "b"};
    CHECK(i.components() == expected_components);
    std::vector<std::pair<PortAddress, Address>> expected_edges{{PortAddress("ptr", "a"), {"b", "c"}}};
    CHECK(i.oriented_binary_operations() == expected_edges);
}
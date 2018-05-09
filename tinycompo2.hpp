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

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "test/doctest.h"

// ~*~ TinyCompo 2 ~*~ ------------------------------------------------------------------------------------------------------
// TinyCompo 2 is a prototype of a possible complete rewrite of TinyCompo
// it aims at improving TinyCompo's structure by making it component-based
// it also aims at minimizing component overhead by separating the component itself from its component-related metadata

// ~*~ Interfaces ~*~ -------------------------------------------------------------------------------------------------------
struct AllocatedObjects {
    virtual ~AllocatedObjects() = default;
};

struct Component : public AllocatedObjects {};

struct ComponentData : public AllocatedObjects {
    std::string name;
    Component& ref;
    ComponentData(std::string name, Component& ref) : name(name), ref(ref) {}
};

struct _Allocation {
    std::unique_ptr<AllocatedObjects> data;
    std::vector<Component*> components;
};

struct _Allocator {
    std::function<_Allocation()> allocate;
};

struct _DataAllocator {
    std::function<_Allocation(_Allocation&)> allocate;
};

struct Model {
    std::map<std::string, _Allocator> allocators;
    std::map<std::string, _DataAllocator> data_allocators;

    // most easy to read function I ever wrote
    template <class Type, class... Args>
    void component(std::string name, Args... args) {
        auto allocate_component = [args...]() {
            _Allocation alloc{std::unique_ptr<AllocatedObjects>(dynamic_cast<AllocatedObjects*>(new Type(args...))),
                              std::vector<Component*>()};
            alloc.components.push_back(dynamic_cast<Component*>(alloc.data.get()));
            return alloc;
        };

        allocators[name] = {allocate_component};

        auto allocate_component_data = [name](_Allocation& a) {
            return _Allocation{std::unique_ptr<AllocatedObjects>(dynamic_cast<AllocatedObjects*>(
                                   new ComponentData(name, *dynamic_cast<Component*>(a.data.get())))),
                               std::vector<Component*>()};
        };

        data_allocators[name] = {allocate_component_data};
    }
};

struct Assembly {
    Assembly(Model& m) {}
};

// ~*~ Test ~*~ -------------------------------------------------------------------------------------------------------------
struct MyCompo : public Component {
    int data;
    MyCompo(int data) : data(data) {}
};

TEST_CASE("main") {
    Model m;
    m.component<MyCompo>("compo1", 3);
}

/* Copyright Vincent Lanore (2017) - vincent.lanore@gmail.com

This software is a computer program whose purpose is to provide the
necessary classes to write ligntweight component-based c++ applications.

This software is governed by the CeCILL license under French law and
abiding by the rules of distribution of free software. You can use,
modify and/ or redistribute the software under the terms of the CeCILL
license as circulated by CEA, CNRS and INRIA at the following URL
"http://www.cecill.info".

As a counterpart to the access to the source code and rights to copy,
modify and redistribute granted by the license, users are provided only
with a limited warranty and the software's author, the holder of the
economic rights, and the successive licensors have only limited
liability.

In this respect, the user's attention is drawn to the risks associated
with loading, using, modifying and/or developing or reproducing the
software by the user in light of its specific status of free software,
that may mean that it is complicated to manipulate, and that also
therefore means that it is reserved for developers and experienced
professionals having in-depth computer knowledge. Users are therefore
encouraged to load and test the software's suitability as regards their
requirements in conditions enabling the security of their systems and/or
data to be ensured and, more generally, to use and operate it in the
same conditions as regards security.

The fact that you are presently reading this means that you have had
knowledge of the CeCILL license and that you accept its terms.*/

#ifndef MODEL_HPP
#define MODEL_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include "doctest.h"

/*
====================================================================================================
  Component class
==================================================================================================*/
class Component {
  public:
    virtual std::string _debug() = 0;
};

/*
============================================== TEST ==============================================*/
TEST_CASE("Basic component tests.") {
    class MyCompo : public Component {
      public:
        std::string _debug() override { return "MyCompo"; }
    };

    MyCompo compo;  // mostly to check that the class is not virtual
    CHECK(compo._debug() == "MyCompo");
}

/*
====================================================================================================
  _Component class
==================================================================================================*/
template <class T>
class _Type {};

class _Component {
  public:
    template <class T, class... Args>
    _Component(_Type<T>, Args&&... args) {
        // stores a lambda that creates a new object of type T with provided args and returns a
        // unique pointer to the newly created object
        _constructor = [=]() {
            return std::unique_ptr<Component>(
                dynamic_cast<Component*>(new T(std::forward<const Args>(args)...)));
        };
    }

    std::function<std::unique_ptr<Component>()> _constructor;  // stores the component constructor
};

/*
============================================== TEST ==============================================*/
TEST_CASE("Assembly class tests.") {
    class MyClass : public Component {
      public:
        std::string _debug() { return "MyClass"; }
        int i{1};
        int j{1};
        MyClass(int i, int j) : i(i), j(j) {}
    };

    _Component compo(_Type<MyClass>(), 3, 4);  // create _Component object
    auto ptr = compo._constructor();           // instantiate actual object
    auto ptr2 = dynamic_cast<MyClass*>(ptr.get());
    CHECK(ptr2->i == 3);
    CHECK(ptr2->j == 4);
}

/*
====================================================================================================
  Assembly class
==================================================================================================*/
class Assembly {
    std::vector<_Component> components;
    std::vector<std::unique_ptr<Component>> instances;

  public:
    template <class T, class ...Args>
    void component(Args&&... args) {
        components.emplace_back(_Type<T>(), std::forward<Args>(args)...);
    }

    void instantiate() {
        for (auto c : components) {
            instances.emplace_back(c._constructor());
        }
    }

    Component* ptr_to_instance(int index) {
        return instances.at(index).get();
    }
};

/*
============================================== TEST ==============================================*/
TEST_CASE("Basic test.") {
    class MyClass : public Component {
    public:
        std::string _debug() { return "MyClass"; }
        int i{1};
        int j{1};
        MyClass(int i, int j) : i(i), j(j) {}
    };
    Assembly a;
    a.component<MyClass>(3, 4);
    a.instantiate();
    auto ptr = dynamic_cast<MyClass*>(a.ptr_to_instance(0));
    CHECK(ptr->i == 3);
    CHECK(ptr->j == 4);
}

#endif  // MODEL_HPP

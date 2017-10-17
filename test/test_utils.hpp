/* Copyright or © or Copr. Centre National de la Recherche Scientifique (CNRS) (2017/05/03)
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

#ifndef TEST_UTILS
#define TEST_UTILS

#include "../tinycompo.hpp"
#include "doctest.h"

#define TINYCOMPO_TEST_ERRORS                  \
    std::stringstream error_short, error_long; \
    TinycompoDebug::set_stream(error_long);    \
    try

#define TINYCOMPO_TEST_MORE_ERRORS          \
    error_short.str("");                    \
    error_long.str("");                     \
    TinycompoDebug::set_stream(error_long); \
    try

#define TINYCOMPO_TEST_ERRORS_END(short, long) \
    catch (TinycompoException & e) {           \
        error_short << e.what();               \
    }                                          \
    TinycompoDebug::set_stream(std::cerr);     \
    CHECK(error_short.str() == short);         \
    CHECK(error_long.str() == long);

#define TINYCOMPO_THERE_WAS_AN_ERROR       \
    catch (TinycompoException & e) {       \
        error_short << e.what();           \
    }                                      \
    TinycompoDebug::set_stream(std::cerr); \
    CHECK(error_short.str() != "");

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

class IntInterface {
  public:
    virtual int get() const = 0;
};

class MyInt : public Component, public IntInterface {
  public:
    int i{1};
    explicit MyInt(int i = 0) : i(i) { port("set", &MyInt::set); }
    std::string _debug() const override { return "MyInt"; }
    int get() const override { return i; }
    void set(int i2) { i = i2; }
};

class MyIntProxy : public Component, public IntInterface {
    IntInterface* ptr{nullptr};

  public:
    MyIntProxy() { port("ptr", &MyIntProxy::set_ptr); }
    void set_ptr(IntInterface* ptrin) { ptr = ptrin; }
    std::string _debug() const override { return "MyIntProxy"; }
    int get() const override { return 2 * ptr->get(); }
};

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

    IntReducer() { port("ptr", &IntReducer::addPtr); }
};

#endif  // TEST_UTILS

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

#include <chrono>
#include <cstdlib>
#include "tinycompo.hpp"

#define ITERATIONS 1000000000

struct GetInt {
    virtual int getInt() = 0;
};

struct RandInt : public GetInt, public Component {
    int state{5};
    int getInt() final {
        state += 5;
        return state;
    }
};

template <class PtrClass>
struct User : public Component {
    int sum{0};
    PtrClass* ptr{nullptr};
    User() {
        port("ptr", &User::setPtr);
        port("go", &User::go);
    }
    void setPtr(PtrClass* ptrin) { ptr = ptrin; }
    void go() {
        for (int i = 0; i < ITERATIONS; i++) {
            sum += ptr->getInt();
        }
    }
};

double measure(std::function<void()> f) {
    auto begin = std::chrono::high_resolution_clock::now();
    f();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / float(ITERATIONS);
}

int main() {
    srand(time(NULL));

    Model<> model;
    model.component<RandInt>("provider");
    model.component<User<RandInt>>("user");
    model.connect<Use<RandInt>>(PortAddress("ptr", "user"), Address("provider"));

    Model<> model2;
    model2.component<RandInt>("provider");
    model2.component<User<GetInt>>("user");
    model2.connect<Use<GetInt>>(PortAddress("ptr", "user"), Address("provider"));

    Assembly<> assembly(model);
    Assembly<> assembly2(model2);

    // cache heating
    assembly.call("user", "go");
    assembly2.call("user", "go");

    double run1 = measure([&]() { assembly.call("user", "go"); });
    double run2 = measure([&]() { assembly2.call("user", "go"); });

    std::cout << run1 << " ns/it\n";
    std::cout << run2 << " ns/it\n";

    double diff = run2 - run1;
    std::cout << "Difference: " << diff << " ns/it\n";
}

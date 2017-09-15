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
    model.connect<UseProvide<RandInt>>(Address("user"), "ptr", Address("provider"));

    Model<> model2;
    model2.component<RandInt>("provider");
    model2.component<User<GetInt>>("user");
    model2.connect<UseProvide<GetInt>>(Address("user"), "ptr", Address("provider"));

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

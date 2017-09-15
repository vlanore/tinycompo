#include <chrono>
#include <cstdlib>
#include "tinycompo.hpp"

struct GetInt {
    virtual int getInt() = 0;
};

struct GetIntState : public GetInt {
    int state{7};
};

struct RandInt : public GetIntState, public Component {
    // int state{7};
    int getInt() final {
        state = (state * state) + 17;
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
        for (int i = 0; i < 100000000; i++) {
            sum += ptr->getInt();
        }
        std::cout << sum << '\n';
    }
};

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

    // cache heating
    Assembly<> assembly2(model2);
    auto begin = std::chrono::high_resolution_clock::now();
    assembly2.call("user", "go");
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / 100000000.0 << "ns/it"
              << std::endl;

    Assembly<> assembly(model);
    begin = std::chrono::high_resolution_clock::now();
    assembly.call("user", "go");
    end = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / 100000000.0 << "ns/it"
              << std::endl;

    Assembly<> assembly3(model2);
    begin = std::chrono::high_resolution_clock::now();
    assembly3.call("user", "go");
    end = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / 100000000.0 << "ns/it"
              << std::endl;
}

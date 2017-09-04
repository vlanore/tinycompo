#include <stdio.h>
#include <sstream>
#include "tinycompo.hpp"

struct IntHolder : public Component {
    explicit IntHolder(int in = 1) : myint(in) {}

    std::string _debug() const override {
        std::stringstream ss;
        ss << "IntHolder(" << myint << ")";
        return ss.str();
    }

    int myint = 0;
};

// Class that specializes Array with a custom contructor
struct MyArray : public Array<IntHolder> {
    explicit MyArray(int nb) : Array<IntHolder>(0) {
        for (int i = 0; i < nb; i++) {
            component<IntHolder>(i, i + 1);
        }
    }
};

int main() {
    Model<> mymodel;
    mymodel.composite<MyArray>("array", 10);

    Assembly<> myassembly(mymodel);
    myassembly.print_all();
}

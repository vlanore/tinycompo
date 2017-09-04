#include <stdio.h>
#include <sstream>
#include "tinycompo.hpp"

class IntHolder : public Component {
  public:
    explicit IntHolder(int in = 1) : myint(in) { port("print", &IntHolder::print); }
    std::string _debug() const override {
        std::stringstream ss;
        ss << "IntHolder(" << myint << ")";
        return ss.str();
    }
    int myint = 0;
    void print() { printf("%d", myint); }
};

// Class that specializes Array with a custom contructor
class MyArray : public Array<IntHolder> {
  public:
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

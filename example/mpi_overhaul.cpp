#include <mpi.h>
#include "tinycompo.hpp"

using namespace std;

struct Hello : public tc::Component {
    Hello() {
        port("hello", &Hello::hello);
    }

    void hello() {
        cout << "Hello\n";
    }
};

int main() {
    tc::Model model;
    model.component<Hello>("compo");

    tc::Assembly assembly(model);
    assembly.call("compo", "hello");
}

#include <mpi.h>
#include "tinycompo.hpp"

using namespace std;
using namespace tc;

/*
=============================================================================================================================
  ~*~ Random tetsing stuff ~*~
===========================================================================================================================*/
struct MyCompo : public Component {
    MyCompo(int i) { cout << "Hello " + to_string(i) + "\n"; }
};

/*
=============================================================================================================================
  ~*~ MPI classes ~*~
===========================================================================================================================*/
using Interval = int;
class MPIAssembly;

class MPIModel {
    Model model;
    map<Address, Interval> intervals;
    friend MPIAssembly;

  public:
    template <class T, class... Args>
    void component(Address address, Interval interval, Args&&... args) {
        model.component<T>(address, std::forward<Args>(args)...);
        intervals[address] = interval;
    }
};

class MPIAssembly : public Component {
    Assembly assembly;

  public:
    MPIAssembly(MPIModel model) : assembly(model.model) {}
};

/*
=============================================================================================================================
  ~*~ main ~*~
===========================================================================================================================*/
int main() {
    MPIModel model;
    model.component<MyCompo>("a", 1, 17);

    MPIAssembly assembly(model);
}

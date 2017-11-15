#include <mpi.h>
#include "tinycompo.hpp"

using namespace std;
using namespace tc;

/*
=============================================================================================================================
  ~*~ Random testing stuff ~*~
===========================================================================================================================*/
struct MyCompo : public Component {
    MyCompo(int i) { cout << "Hello " + to_string(i) + "\n"; }
};

/*
=============================================================================================================================
  ~*~ MPI Model ~*~
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

/*
=============================================================================================================================
  ~*~ Option class ~*~
===========================================================================================================================*/
template <class T>
class Option {
    list<T> data;  // please don't laugh

  public:
    Option() = default;
    Option(const T& data) : data(1, data) {}

    template <class... Args>
    Option(Args&&... args) {
        data.emplace_front(forward<Args>(args)...);
    }

    void operator=(const T& new_data) { data.size() == 0 ? data.push_back(new_data) : data.at(0) = new_data; }

    bool operator!() { return data.size() == 1; }
    T& operator*() { return data.front(); }
    T* operator->() { return &data.front(); }
};

/*
=============================================================================================================================
  ~*~ MPI Assembly ~*~
===========================================================================================================================*/
class MPIAssembly : public Component {
    Option<Assembly> assembly;

  public:
    MPIAssembly(MPIModel model) : assembly(model.model) {}
};

/*
=============================================================================================================================
  ~*~ main ~*~
===========================================================================================================================*/
int main() {
    MPI_Init(NULL, NULL);

    MPIModel model;
    model.component<MyCompo>("a", 1, 17);
    model.component<MyCompo>("b", 0, 13);

    MPIAssembly assembly(model);

    MPI_Finalize();
}

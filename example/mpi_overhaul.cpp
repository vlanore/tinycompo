#include <mpi.h>
#include "tinycompo.hpp"

using namespace std;
using namespace tc;

/*
=============================================================================================================================
  ~*~ Random testing stuff ~*~
===========================================================================================================================*/
struct MyCompo : public Component {
    MyCompo(int i) {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        printf("<%d> Hello %d\n", rank, i);
    }
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

    template <class... Args>
    void set(Args&&... args) {
        data.emplace_front(forward<Args>(args)...);
    }

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
    int rank;

  public:
    MPIAssembly(MPIModel model) {
        MPI_Init(NULL, NULL);
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        for (auto i : model.intervals) {
            if (i.second != rank) {
                model.model.remove(i.first);
            }
        }
        assembly.set(model.model);
    }

    ~MPIAssembly() { MPI_Finalize(); }
};

/*
=============================================================================================================================
  ~*~ main ~*~
===========================================================================================================================*/
int main() {
    MPIModel model;
    model.component<MyCompo>("a", 1, 17);
    model.component<MyCompo>("b", 0, 13);

    MPIAssembly assembly(model);
}

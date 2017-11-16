#include <mpi.h>
#include "tinycompo.hpp"

using namespace std;
using namespace tc;

/*
=============================================================================================================================
  ~*~ MPICore class ~*~
===========================================================================================================================*/
class MPICore {
    const vector<int> colors{31, 32, 33, 34, 35, 36, 91, 92, 93, 94, 95, 96};

  public:
    int rank{-1};
    int size{-1};
    MPICore() {
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    }
    template <class... Args>
    void message(const std::string& format, Args... args) {
        string format2 = "\e[" + to_string(colors[rank % colors.size()]) + "m<%d/%d> " + format + "\e[0m\n";
        printf(format2.c_str(), rank, size, args...);
    }
};

/*
=============================================================================================================================
  ~*~ ProcessSet ~*~
===========================================================================================================================*/
struct ProcessSet {
    function<bool(int)> contains;

    ProcessSet() : contains([](int) { return false; }) {}
    ProcessSet(int p) : contains([p](int i) { return i == p; }) {}
    ProcessSet(int p, int q) : contains([p, q](int i) { return i >= p and i < q; }) {}
    template <class F>
    ProcessSet(F f) : contains(f) {}
};

namespace process {
ProcessSet all{[](int) { return true; }};
ProcessSet odd{[](int i) { return i % 2 == 1; }};
ProcessSet even{[](int i) { return i % 2 == 0; }};
ProcessSet interval(int i, int j) { return ProcessSet{i, j}; }
ProcessSet up_from(int p) {
    return ProcessSet([p](int i) { return i >= p; });
}
}

/*
=============================================================================================================================
  ~*~ MPI Model ~*~
===========================================================================================================================*/
class MPIAssembly;

class MPIModel {
    Model model;
    map<Address, ProcessSet> intervals;
    friend MPIAssembly;

  public:
    template <class T, class... Args>
    void component(Address address, ProcessSet interval, Args&&... args) {
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
            if (!i.second.contains(rank)) {
                model.model.remove(i.first);
            }
        }
        assembly.set(model.model);
    }

    ~MPIAssembly() { MPI_Finalize(); }
};

/*
=============================================================================================================================
  ~*~ Random testing stuff ~*~
===========================================================================================================================*/
class MyCompo : public Component {
    MPICore core;

  public:
    MyCompo(string s) { core.message("Hello %s", s.c_str()); }
};

/*
=============================================================================================================================
  ~*~ main ~*~
===========================================================================================================================*/
int main() {
    MPIModel model;
    model.component<MyCompo>("a", 1, "1");
    model.component<MyCompo>("b", 0, "0");
    model.component<MyCompo>("c", process::all, "all");
    model.component<MyCompo>("d", process::up_from(2), "at least 2");
    model.component<MyCompo>("e", {2, 4}, "2 to 4");
    model.component<MyCompo>("f", process::interval(2, 4), "2 to 4 again");
    model.component<MyCompo>("g", process::odd, "odd");
    model.component<MyCompo>("h", process::even, "even");

    MPIAssembly assembly(model);
}

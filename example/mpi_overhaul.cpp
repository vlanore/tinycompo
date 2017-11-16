#include <mpi.h>
#include "tinycompo.hpp"

using namespace std;
using namespace tc;

struct Go {
    virtual void go() = 0;
};

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
  ~*~ MPIPort ~*~
===========================================================================================================================*/
struct MPIPort {
    int proc{-1};
    int tag{-1};
    MPIPort() = default;
    MPIPort(int p, int t) : proc(p), tag(t) {}

    void send(int data) { MPI_Send(&data, 1, MPI_INT, proc, tag, MPI_COMM_WORLD); }
    void send(void* data, int count, MPI_Datatype type) { MPI_Send(data, count, type, proc, tag, MPI_COMM_WORLD); }

    void receive(int& data) { MPI_Recv(&data, 1, MPI_INT, proc, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE); }
    void receive(void* data, int count, MPI_Datatype type) {
        MPI_Recv(data, count, type, proc, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
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
struct _MPIInit {
    _MPIInit() { MPI_Init(NULL, NULL); }
    ~_MPIInit() { MPI_Finalize(); }
};

class MPIAssembly : public Component {
    static _MPIInit init;

    Option<Assembly> assembly;
    int rank;

  public:
    MPIAssembly(MPIModel model) {
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        for (auto i : model.intervals) {
            if (!i.second.contains(rank)) {
                model.model.remove(i.first);
            }
        }
        assembly.set(model.model);
    }

    void barrier() { MPI_Barrier(MPI_COMM_WORLD); }

    void call(PortAddress port, ProcessSet processes) {
        if (processes.contains(rank)) {
            assembly->call(port);
        }
    }
};

#ifndef TC_MPI_INITIALIZED
#define TC_MPI_INITIALIZED
_MPIInit MPIAssembly::init{};
#endif

/*
=============================================================================================================================
  ~*~ Random testing stuff ~*~
===========================================================================================================================*/
class MyCompoOdd : public Component, public Go {
    MPICore core;
    MPIPort my_port;

  public:
    MyCompoOdd() { port("go", &MyCompoOdd::go); }

    void go() override {
        core.message("hello");

        // my_port.send(core.rank);
        // core.message("sent %d", core.rank);

        // int receive_msg;
        // my_port.receive(receive_msg);
        // core.message("received %d", receive_msg);
    }
};

class MyCompoEven : public Component, public Go {
    MPICore core;
    MPIPort my_port;

  public:
    MyCompoEven() { port("go", &MyCompoEven::go); }

    void go() override {
        core.message("hello");

        // int receive_msg;
        // my_port.receive(receive_msg);
        // core.message("received %d", receive_msg);

        // my_port.send(core.rank);
        // core.message("sent %d", core.rank);
    }
};

/*
=============================================================================================================================
  ~*~ main ~*~
===========================================================================================================================*/
int main() {
    MPIModel model;
    model.component<MyCompoOdd>("odd", process::odd);
    model.component<MyCompoEven>("even", process::even);

    MPIAssembly assembly(model);
    assembly.call(PortAddress("go", "odd"), process::odd);
    assembly.call(PortAddress("go", "even"), process::even);
}

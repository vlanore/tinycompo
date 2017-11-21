/* Copyright or © or Copr. Centre National de la Recherche Scientifique (CNRS) (2017/05/03)
Contributors:
- Vincent Lanore <vincent.lanore@gmail.com>

This software is a computer program whose purpose is to provide the necessary classes to write ligntweight component-based
c++ applications.

This software is governed by the CeCILL-B license under French law and abiding by the rules of distribution of free software.
You can use, modify and/ or redistribute the software under the terms of the CeCILL-B license as circulated by CEA, CNRS and
INRIA at the following URL "http://www.cecill.info".

As a counterpart to the access to the source code and rights to copy, modify and redistribute granted by the license, users
are provided only with a limited warranty and the software's author, the holder of the economic rights, and the successive
licensors have only limited liability.

In this respect, the user's attention is drawn to the risks associated with loading, using, modifying and/or developing or
reproducing the software by the user in light of its specific status of free software, that may mean that it is complicated
to manipulate, and that also therefore means that it is reserved for developers and experienced professionals having in-depth
computer knowledge. Users are therefore encouraged to load and test the software's suitability as regards their requirements
in conditions enabling the security of their systems and/or data to be ensured and, more generally, to use and operate it in
the same conditions as regards security.

The fact that you are presently reading this means that you have had knowledge of the CeCILL-B license and that you accept
its terms.*/

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
    int rank, size;
    MPICore(int rank, int size) : rank(rank), size(size) {}
    template <class... Args>
    void message(const std::string& format, Args... args) {
        string format2 = "\e[" + to_string(colors[rank % colors.size()]) + "m<%d/%d> " + format + "\e[0m\n";
        printf(format2.c_str(), rank, size, args...);
    }
};

/*
=============================================================================================================================
  ~*~ MPIContext class ~*~
===========================================================================================================================*/
struct MPIContext {
    static int rank, size;

    MPIContext(int argc, char** argv) {
        int initialized;
        MPI_Initialized(&initialized);
        if (initialized == 0) {
            MPI_Init(&argc, &argv);
        } else {
            throw TinycompoException("trying to instantiate several MPIContext objects");
        }
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &size);
    }

    MPIContext(const MPIContext&) = delete;

    ~MPIContext() { MPI_Finalize(); }

    static MPICore core() { return MPICore(rank, size); }
};

#ifndef MPICONTEXT_INIT
#define MPICONTEXT_INIT
int MPIContext::rank{-1};
int MPIContext::size{-1};
#endif

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

struct RelativeProcess {
    function<int(int)> process_modifier;

    template <class F>
    RelativeProcess(F f) : process_modifier(f) {}

    set<int> all_origins(ProcessSet processes) {
        auto core = MPIContext::core();
        set<int> result;
        for (auto p = 0; p < core.size; p++) {
            if (processes.contains(p) and (process_modifier(p) % core.size) == core.rank) {
                result.insert(p);
            }
        }
        return result;
    }
};

struct P2P {
    static void connect(Model& model, PortAddress user, ProcessSet user_process, PortAddress provider,
                        RelativeProcess provider_process) {
        auto core = MPIContext::core();

        // user-side
        if (user_process.contains(core.rank)) {
            model.connect<Set>(user, provider_process.process_modifier(core.rank) % core.size, 0);
        }

        // provider-side
        auto origin_processes = provider_process.all_origins(user_process);
        for (auto p : origin_processes) {
            model.connect<Set>(provider, p, 0);
        }
    }
};

/*
=============================================================================================================================
  ~*~ MPI Model ~*~
===========================================================================================================================*/
template <class C>
struct CondCompo {
    template <class... Args>
    static void connect(Model& model, const std::string& name, ProcessSet process, Args&&... args) {
        auto core = MPIContext::core();
        if (process.contains(core.rank)) {
            model.component<C>(name, std::forward<Args>(args)...);
        }
    }
};

class MPIAssembly;

class MPIModel {
    Model model;
    MPICore core;
    friend MPIAssembly;

  public:
    MPIModel() : core(MPIContext::core()) {}

    template <class T, class... Args>
    void component(Address address, ProcessSet process, Args&&... args) {
        model.meta_component<CondCompo<T>>(address, process, std::forward<Args>(args)...);
    }

    template <class T, class... Args>
    void connect(Args&&... args) {
        model.meta_connect<T>(std::forward<Args>(args)...);
    }
};

/*
=============================================================================================================================
  ~*~ MPI Assembly ~*~
===========================================================================================================================*/
class MPIAssembly : public Component {
    Assembly assembly;
    MPICore core;

  public:
    MPIAssembly(MPIModel model) : assembly(model.model), core(MPIContext::core()) {}

    void barrier() { MPI_Barrier(MPI_COMM_WORLD); }

    void call(PortAddress port, ProcessSet processes) {
        if (processes.contains(core.rank)) {
            assembly.call(port);
        }
    }
};

/*
=============================================================================================================================
  ~*~ Random testing stuff ~*~
===========================================================================================================================*/
class MyCompoOdd : public Component, public Go {
    MPICore core;
    MPIPort my_port;
    void set_port(int target, int tag) { my_port = MPIPort(target, tag); }

  public:
    MyCompoOdd() : core(MPIContext::core()) {
        port("go", &MyCompoOdd::go);
        port("port", &MyCompoOdd::set_port);
    }

    void go() override {
        core.message("hello");

        my_port.send(core.rank);
        core.message("sent %d", core.rank);

        int receive_msg;
        my_port.receive(receive_msg);
        core.message("received %d", receive_msg);
    }
};

class MyCompoEven : public Component, public Go {
    MPICore core;
    MPIPort my_port;
    void set_port(int target, int tag) { my_port = MPIPort(target, tag); }

  public:
    MyCompoEven() : core(MPIContext::core()) {
        port("go", &MyCompoEven::go);
        port("port", &MyCompoEven::set_port);
    }

    void go() override {
        core.message("hello");

        int receive_msg;
        my_port.receive(receive_msg);
        core.message("received %d", receive_msg);

        my_port.send(core.rank);
        core.message("sent %d", core.rank);
    }
};

/*
=============================================================================================================================
  ~*~ main ~*~
===========================================================================================================================*/
int main(int argc, char** argv) {
    MPIContext context(argc, argv);

    MPIModel model;
    model.component<MyCompoOdd>("odd", process::odd);
    model.component<MyCompoEven>("even", process::even);
    model.connect<P2P>(PortAddress("port", "odd"), process::odd, PortAddress("port", "even"),
                       RelativeProcess([](int p) { return p + 1; }));

    MPIAssembly assembly(model);
    assembly.call(PortAddress("go", "odd"), process::odd);
    assembly.call(PortAddress("go", "even"), process::even);
}

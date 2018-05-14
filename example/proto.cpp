// #define USE_MPI

#ifdef USE_MPI
#include <mpi.h>
#endif
#include <functional>
#include <future>
#include <iostream>
#include <sstream>
#include <thread>

using namespace std;

int my_size{-1}, my_rank{-1};

string info() {
    stringstream ss;
    ss << "process " << my_rank << ", thread " << this_thread::get_id();
    return ss.str();
}

// ~*~ Data Handles ~*~ -----------------------------------------------------------------------------------------------------
class SeqHandle {
    int data;

  public:
    template <class... Args>
    SeqHandle(Args... args) : data(bind(args...)()) {}
    int get() { return data; }
};

class ThreadHandle {
    future<int> data;

  public:
    template <class... Args>
    ThreadHandle(Args... args) : data(async(args...)) {}
    int get() { return data.get(); }
};

#ifdef USE_MPI
class MPIHandle {
  public:
    template <class... Args>
    MPIHandle(Args...) {
        MPI_Send(NULL, 0, MPI_INT, 1, 0, MPI_COMM_WORLD);
    }

    int get() {
        int data;
        MPI_Recv(&data, 1, MPI_INT, 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        return data;
    }
};
#endif

// ~*~ Objects ~*~ ----------------------------------------------------------------------------------------------------------
class Worker {
    int f() {
        cout << "Computed function on " << info() << endl;
        return 19;
    }

  public:
    template <class DataHandle>
    DataHandle get_handle() {
        return DataHandle(&Worker::f, this);
    }
};

template <class DataHandle>
class Master {
    Worker& worker;

  public:
    Master(Worker& worker) : worker(worker) {}

    void go() {
        auto handle = worker.get_handle<DataHandle>();
        cout << "Got data " << handle.get() << " on " << info() << endl;
    }
};

#ifdef USE_MPI
class MPIDispatcher {
    Worker& worker;

  public:
    MPIDispatcher(Worker& worker) : worker(worker) {}

    template <class Handle>
    void go() {
        MPI_Recv(NULL, 0, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        auto handle = worker.get_handle<Handle>();
        int data = handle.get();
        MPI_Send(&data, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
    }
};
#endif

// ~*~ Main ~*~ -------------------------------------------------------------------------------------------------------------
int main() {
    // using Handle = MPIHandle;
    // using Handle = SeqHandle;
    using Handle = ThreadHandle;

#ifdef USE_MPI
    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &my_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
#endif

    Worker w;
#ifdef USE_MPI
    if (my_rank == 1) {
        MPIDispatcher dispatch(w);
        dispatch.go<SeqHandle>();
    }
    if (my_rank == 0)
#endif
    {
        Master<Handle> m(w);
        m.go();
    }

#ifdef USE_MPI
    MPI_Finalize();
#endif
}

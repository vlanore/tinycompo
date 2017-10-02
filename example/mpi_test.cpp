#include <mpi.h>
#include <stdio.h>
#include "tinycompo.hpp"

using namespace std;

struct MPIConfig {
    int rank{-1};
    int size{-1};
    MPIConfig() {
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    }
};

struct MPIPort {
    int proc{-1};
    int tag{-1};
    MPIPort() = default;
    MPIPort(int p, int t) : proc(p), tag(t) {}
    void send(void* data, int count, MPI_Datatype type) { MPI_Send(data, count, type, proc, tag, MPI_COMM_WORLD); }
    void receive(void* data, int count, MPI_Datatype type) {
        MPI_Recv(data, count, type, proc, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
};

// struct Sender : public Component {
//     MPIPort receiver{1, 17};
//     Sender() { port("hello", &Sender::hello); }
//     void hello() {
//         int rank = MPIConfig().rank, size = MPIConfig().size;
//         printf("<%d/%d> Hello, I am a sender!\n", rank, size);
//         int number = 17;
//         receiver.send(&number, 1, MPI_INT);
//     }
// };

// struct Receiver : public Component {
//     MPIPort sender{0, 17};
//     Receiver() { port("hello", &Receiver::hello); }
//     void hello() {
//         int rank = MPIConfig().rank, size = MPIConfig().size;
//         printf("<%d/%d> Hello, I am a receiver!\n", rank, size);
//         int number;
//         sender.receive(&number, 1, MPI_INT);
//         printf("<%d/%d> Received %d from process 0\n", rank, size, number);
//     }
// };

class MPIReducer : public Component, public MPIConfig {
    vector<MPIPort> ports;
    void addPort(MPIPort port) { ports.push_back(port); }

  public:
    MPIReducer() {
        port("ports", &MPIReducer::addPort);
        port("go", &MPIReducer::go);
    }

    void go() {
        int total{0};
        for (auto p : ports) {
            int data;
            p.receive(&data, 1, MPI_INT);
            total += data;
        }
        printf("<%d/%d> Total is %d\n", rank, size, total);
    }
};

class LocalSender : public Component, public MPIConfig {
    MPIPort reducer;
    int data;

  public:
    explicit LocalSender(int data = 7) : data(data) {
        port("port", &LocalSender::reducer);
        port("go", &LocalSender::go);
    }

    void go() {
        printf("<%d/%d> Sending %d to reducer!\n", rank, size, data);
        reducer.send(&data, 1, MPI_INT);
        printf("<%d/%d> Done!\n", rank, size);
    }
};

int main() {
    MPI_Init(NULL, NULL);
    int rank = MPIConfig().rank, size = MPIConfig().size;

    Model<> model;
    if (rank == 0) {
        model.component<MPIReducer>("compo");
        for (int i = 0; i < size - 1; i++) {
            model.connect<Set>(PortAddress("ports", "compo"), MPIPort{i + 1, i + 1});
        }
    } else {
        model.component<LocalSender>("compo", (rank * 11 + 3) % 8);
        model.connect<Set>(PortAddress("port", "compo"), MPIPort{0, rank});
    }

    Assembly<> assembly(model);
    assembly.call("compo", "go");

    MPI_Finalize();
}

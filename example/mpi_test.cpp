#include <mpi.h>
#include <stdio.h>
#include "tinycompo.hpp"

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
    MPIPort(int p, int t) : proc(p), tag(t) {}
    void send(void* data, int count, MPI_Datatype type) { MPI_Send(data, count, type, proc, tag, MPI_COMM_WORLD); }
    void receive(void* data, int count, MPI_Datatype type) {
        MPI_Recv(data, count, type, proc, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
};

struct Sender : public Component, public MPIConfig {
    MPIPort receiver{1, 17};
    Sender() { port("hello", &Sender::hello); }
    void hello() {
        printf("<%d/%d> Hello, I am a sender!\n", rank, size);
        int number = 17;
        receiver.send(&number, 1, MPI_INT);
    }
};

struct Receiver : public Component, public MPIConfig {
    MPIPort sender{0, 17};
    Receiver() { port("hello", &Receiver::hello); }
    void hello() {
        printf("<%d/%d> Hello, I am a receiver!\n", rank, size);
        int number;
        sender.receive(&number, 1, MPI_INT);
        printf("<%d/%d> Received %d from process 0\n", rank, size, number);
    }
};

int main() {
    MPI_Init(NULL, NULL);

    Model<> model;
    if (MPIConfig().rank == 0) model.component<Sender>("compo");
    if (MPIConfig().rank == 1) model.component<Receiver>("compo");

    Assembly<> assembly(model);
    assembly.call("compo", "hello");

    MPI_Finalize();
}

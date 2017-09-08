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

struct Sender : public Component, public MPIConfig {
    Sender() { port("hello", &Sender::hello); }
    void hello() {
        printf("<%d/%d> Hello, I am a sender!\n", rank, size);
        int number = 17;
        MPI_Send(&number, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
    }
};

struct Receiver : public Component, public MPIConfig {
    Receiver() { port("hello", &Receiver::hello); }
    void hello() {
        printf("<%d/%d> Hello, I am a receiver!\n", rank, size);
        int number;
        MPI_Recv(&number, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
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

#include <mpi.h>
#include <stdio.h>
#include <set>
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

class MPIModel : public Model<>, public MPIConfig {
    map<string, bool> resources;

  public:
    void resource(const string& compo, const set<int>& s) { resources[compo] = (s.find(rank) != s.end()); }
    bool local(const string& compo) {
        try {
            return resources.at(compo);
        } catch (std::out_of_range) {
            return false;
        }
    }
};

Model<> emptymodel;

class MPIAssembly : public Assembly<>, public MPIConfig {
  public:
    explicit MPIAssembly(MPIModel& model) : Assembly<>(emptymodel) {
        for (auto& c : model.components) {
            if (model.local(c.first)) {
                instances.emplace(c.first, std::unique_ptr<Component>(c.second._constructor()));
            }
        }
        for (auto& o : model.operations) {
            o._connect(*this);
        }
    }
};

set<int> interval(int start, int end) {
    int aend = (end == -1) ? MPIConfig().size : end;
    set<int> result;
    for (int i = start; i < aend; i++) {
        result.insert(i);
    }
    return result;
}

int main() {
    MPI_Init(NULL, NULL);
    int rank = MPIConfig().rank, size = MPIConfig().size;

    MPIModel model;
    model.component<MPIReducer>("reducer");
    model.resource("reducer", set<int>{0});

    model.component<LocalSender>("sender", (rank * 11 + 3) % 7);
    model.resource("sender", interval(1, -1));

    if (rank == 0) {
        for (int i = 0; i < size - 1; i++) {
            model.connect<Set>(PortAddress("ports", "reducer"), MPIPort{i + 1, i + 1});
        }
    } else {
        model.connect<Set>(PortAddress("port", "sender"), MPIPort{0, rank});
    }

    MPIAssembly assembly(model);
    if (rank == 0) {
        assembly.call("reducer", "go");
    } else {
        assembly.call("sender", "go");
    }

    MPI_Finalize();
}

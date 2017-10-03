#include <mpi.h>
#include <stdio.h>
#include <algorithm>
#include <set>
#include "tinycompo.hpp"

using namespace std;

/*
===================================================================================
  TinyCompoMPI classes
===================================================================================*/
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

class MPIModel : public Model<>, public MPIConfig {
    map<string, vector<int>> resources;

  public:
    void resource(const string& compo, const vector<int>& v) { resources[compo] = v; }
    vector<int> resource(const std::string& compo) {
        try {
            return resources.at(compo);
        } catch (std::out_of_range) {
            return vector<int>();
        }
    }
    bool local(const string& compo) {
        try {
            return find(resources.at(compo).begin(), resources.at(compo).end(), rank) != resources.at(compo).end();
        } catch (std::out_of_range) {
            return false;
        }
    }
};

Model<> emptymodel;

class MPIAssembly : public Assembly<>, public MPIConfig {
    MPIModel& internal_model;

  public:
    explicit MPIAssembly(MPIModel& model) : Assembly<>(emptymodel), internal_model(model) {
        for (auto& c : model.components) {
            if (model.local(c.first)) {
                instances.emplace(c.first, std::unique_ptr<Component>(c.second._constructor()));
            }
        }
        for (auto& o : model.operations) {
            o._connect(*this);
        }
    }

    vector<int> resource(const std::string& compo) { return internal_model.resource(compo); }

    bool local(const std::string& compo) { return internal_model.local(compo); }

    void ccall(const std::string& compo, const std::string& prop) {
        if (local(compo)) {
            call(compo, prop);
        }
    }
};

vector<int> interval(int start, int end) {
    int aend = (end == -1) ? MPIConfig().size : end;
    vector<int> result;
    for (int i = start; i < aend; i++) {
        result.push_back(i);
    }
    return result;
}

struct MPIp2p {
    // allows multiple users but single provider!
    template <class Key>
    static void _connect(Assembly<>& assembly, _PortAddress<Key> user, _PortAddress<Key> provider) {
        MPIAssembly& a = dynamic_cast<MPIAssembly&>(assembly);
        string nuser = user.address.key.get(), nprov = provider.address.key.get();
        string puser = user.prop, pprov = provider.prop;
        int rprov = a.resource(nprov)[0];
        if (a.local(nprov)) {
            for (auto p : a.resource(nuser)) {
                a.at(nprov).set(pprov, MPIPort(p, 17));
            }
        }
        if (a.local(nuser)) {
            a.at(nuser).set(puser, MPIPort(rprov, 17));
        }
    }
};

/*
===================================================================================
  User-defined classes
===================================================================================*/
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

/*
===================================================================================
  Main
===================================================================================*/
int main() {
    MPI_Init(NULL, NULL);
    int rank = MPIConfig().rank;

    MPIModel model;
    model.component<MPIReducer>("reducer");
    model.resource("reducer", vector<int>{0});

    model.component<LocalSender>("sender", (rank * 11 + 3) % 7);
    model.resource("sender", interval(1, -1));

    model.connect<MPIp2p>(PortAddress("port", "sender"), PortAddress("ports", "reducer"));

    MPIAssembly assembly(model);
    assembly.ccall("reducer", "go");
    assembly.ccall("sender", "go");

    MPI_Finalize();
}

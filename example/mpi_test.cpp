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
        string format2 = "\e[" + to_string(colors[rank]) + "m<%d/%d> " + format + "\e[0m\n";
        printf(format2.c_str(), rank, size, args...);
    }
};

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

class MPIModel : public Model<> {
    map<string, vector<int>> resources;
    MPICore core;

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
            return find(resources.at(compo).begin(), resources.at(compo).end(), core.rank) != resources.at(compo).end();
        } catch (std::out_of_range) {
            return false;
        }
    }
};

Model<> emptymodel;

class MPIAssembly : public Assembly<>, public MPICore {
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
    int aend = (end == -1) ? MPICore().size : end;
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
class MPIReducer : public Component {
    vector<MPIPort> ports;
    void addPort(MPIPort port) { ports.push_back(port); }
    int data_buffer{-1};
    MPICore core;

  public:
    MPIReducer() {
        port("ports", &MPIReducer::addPort);
        port("go", &MPIReducer::go);
    }

    void go() {
        int total{0};
        for (auto p : ports) {
            p.receive(data_buffer);
            total += data_buffer;
        }
        core.message("Total is %d", total);
    }
};

class LocalSender : public Component {
    MPIPort reducer;
    MPICore core;
    int data;

  public:
    explicit LocalSender(int data = 7) : data(data) {
        port("port", &LocalSender::reducer);
        port("go", &LocalSender::go);
    }

    void go() {
        core.message("Sending %d to reducer!", data);
        reducer.send(data);
        reducer.send(1);
        core.message("Done!");
    }
};

/*
===================================================================================
  Main
===================================================================================*/
int main() {
    MPI_Init(NULL, NULL);
    int rank = MPICore().rank;

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

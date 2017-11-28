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

#include "tinycompo_mpi.hpp"

using namespace tc;
using namespace std;

class MySender : public Component, public Go {
    MPICore core;
    MPIPort my_port;
    void set_port(int target, int tag) { my_port = MPIPort(target, tag); }

  public:
    MySender() : core(MPIContext::core()) {
        port("go", &MySender::go);
        port("port", &MySender::set_port);
    }

    void go() override {
        my_port.send(core.rank);
        core.message("sent %d", core.rank);
    }
};

class MyReducer : public Component, public Go {
    MPICore core;
    vector<MPIPort> ports;
    void add_port(int target, int tag) { ports.emplace_back(target, tag); }

  public:
    MyReducer() : core(MPIContext::core()) {
        port("go", &MyReducer::go);
        port("ports", &MyReducer::add_port);
    }

    void go() override {
        int acc = 0;
        for (auto p : ports) {
            int receive_msg;
            p.receive(receive_msg);
            core.message("received %d", receive_msg);
            acc += receive_msg;
        }
        core.message("total is %d", acc);
    }
};

class A2A : public Component, public Go {
    MPICommunicator* comm;
    MPICore core;

  public:
    A2A() : core(MPIContext::core()) {
        port("comm", &A2A::comm);
        port("go", &A2A::go);
    }

    void go() override {
        int my_data = rand() % 17;
        core.message("my data is %d", my_data);
        auto data = comm->all_gather(my_data);
        core.message("data sum is %d", accumulate(data.begin(), data.end(), 0));
    }
};

/*
=============================================================================================================================
  ~*~ main ~*~
===========================================================================================================================*/
int main(int argc, char** argv) {
    MPIContext context(argc, argv);

    MPIModel model;
    // model.component<MySender>("workers", process::up_from(1));
    // model.component<MyReducer>("master", process::zero);
    // model.mpi_connect<P2P>(PortAddress("port", "workers"), process::up_from(1), PortAddress("ports", "master"),
    //                        process::to_zero);

    model.comm("oddcomm", process::odd);
    model.component<A2A>("a2a", process::odd);
    model.mpi_connect<UseComm>(PortAddress("comm", "a2a"), process::odd, "oddcomm");

    MPIAssembly assembly(model);
    // assembly.call(PortAddress("go", "workers"));
    // assembly.call(PortAddress("go", "master"));
    assembly.call(PortAddress("go", "a2a"));
}

/* Copyright or © or Copr. Centre National de la Recherche Scientifique (CNRS) (2017/05/03)
Contributors:
- Vincent Lanore <vincent.lanore@gmail.com>

This software is a computer program whose purpose is to provide the necessary classes to write
ligntweight component-based c++ applications.

This software is governed by the CeCILL-B license under French law and abiding by the rules of
distribution of free software. You can use, modify and/ or redistribute the software under the terms
of the CeCILL-B license as circulated by CEA, CNRS and INRIA at the following URL
"http://www.cecill.info".

As a counterpart to the access to the source code and rights to copy, modify and redistribute
granted by the license, users are provided only with a limited warranty and the software's author,
the holder of the economic rights, and the successive licensors have only limited liability.

In this respect, the user's attention is drawn to the risks associated with loading, using,
modifying and/or developing or reproducing the software by the user in light of its specific status
of free software, that may mean that it is complicated to manipulate, and that also therefore means
that it is reserved for developers and experienced professionals having in-depth computer knowledge.
Users are therefore encouraged to load and test the software's suitability as regards their
requirements in conditions enabling the security of their systems and/or data to be ensured and,
more generally, to use and operate it in the same conditions as regards security.

The fact that you are presently reading this means that you have had knowledge of the CeCILL-B
license and that you accept its terms.*/

#include "poisson_gamma_connectors.hpp"

struct PoissonGamma : public Composite {
    static void contents(Model& model, int size) {
        model.component<Exponential>("Sigma");
        model.connect<Set>(PortAddress("paramConst", "Sigma"), 1.0);

        model.component<Exponential>("Theta");
        model.connect<Set>(PortAddress("paramConst", "Theta"), 1.0);

        model.composite<Array<Gamma>>("Omega", size);
        model.connect<MultiProvide<Real>>(PortAddress("paramPtr", "Omega"), Address("Theta"));

        model.composite<Array<Product>>("rate", size);
        model.connect<ArrayOneToOne<Real>>(PortAddress("aPtr", "rate"), Address("Omega"));
        model.connect<MultiProvide<Real>>(PortAddress("bPtr", "rate"), Address("Sigma"));

        model.composite<Array<Poisson>>("X", size);
        model.connect<ArrayOneToOne<Real>>(PortAddress("paramPtr", "X"), Address("rate"));
    }
};

struct Moves : public Composite {
    static void contents(Model& model, int size) {
        model.component<MHMove<Scaling>>("Sigma", 3, 10);
        model.component<MHMove<Scaling>>("Theta", 3, 10);
        model.composite<Array<MHMove<Scaling>>>("Omega", size, 3, 10);
    }
};

int main() {
    Model model;

    // graphical model part
    int size = 5;
    vector<double> data{0, 1, 1, 0, 1};
    model.composite<PoissonGamma>("PG", size);
    model.connect<ArraySet>(PortAddress("clamp", "PG", "X"), data);
    model.connect<ArraySet>(PortAddress("value", "PG", "X"), data);

    // bayesian engine
    model.component<MCMCEngine>("MCMC", 10000);
    model.connect<Use<Sampler>>(PortAddress("sampler", "MCMC"), Address("Sampler"));
    model.connect<Use<MoveScheduler>>(PortAddress("scheduler", "MCMC"), Address("Scheduler"));
    model.connect<ListUse<Real>>(PortAddress("variables", "MCMC"), Address("PG", "Theta"), Address("PG", "Sigma"));

    // output
    model.component<FileOutput>("TraceFile", "tmp_mcmc.trace");
    model.connect<Use<DataStream>>(PortAddress("output", "MCMC"), Address("TraceFile"));

    // sampler
    model.component<MultiSample>("Sampler");
    model.connect<UseAllUnclampedNodes>(PortAddress("register", "Sampler"), Address("PG"));

    // moves
    model.component<MoveScheduler>("Scheduler");
    model.composite<Moves>("Moves", size);
    model.connect<ConnectAllMoves>(Address("Moves"), Address("PG"), Address("Scheduler"));

    // configMoves(model, "PG", "Scheduler", "Scaling(Theta, 3, 10, array Omega), Scaling(Sigma, 3, 10, array X)");

    // model.composite<Array<MHMove<Scaling>>>("OmegaMove", 5, 3, 10);
    // model.connect<ArrayOneToOne<RandomNode>>(PortAddress("node", "OmegaMove"), Address("PG", "Omega"));
    // model.connect<ArrayOneToOne<RandomNode>>(PortAddress("downward", "OmegaMove"), Address("PG", "X"));
    // model.connect<MultiUse<Go>>(PortAddress("move", "Scheduler"), Address("OmegaMove"));

    // RS infrastructure
    model.component<RejectionSampling>("RS", 500000);
    model.connect<Use<Sampler>>(PortAddress("sampler", "RS"), Address("Sampler2"));
    model.connect<MultiUse<RandomNode>>(PortAddress("data", "RS"), Address("PG", "X"));

    model.component<MultiSample>("Sampler2");
    model.connect<UseTopoSortInComposite<RandomNode>>(PortAddress("register", "Sampler2"), Address("PG"));

    // model.connect<UseInComposite<RandomNode>>(PortAddress("register", "Sampler2"), Address("PG"), "Sigma", "Theta",
    // "Omega",
    //                                           "X");

    model.component<FileOutput>("TraceFile2", "tmp_rs.trace");
    model.connect<Use<DataStream>>(PortAddress("output", "RS"), Address("TraceFile2"));

    Model(_Type<PoissonGamma>(), 3).dot_to_file("tmp_pg.dot");
    model.dot_to_file();
    // model.print_representation();

    // instantiate everything!
    Assembly assembly(model);

    assembly.call("MCMC", "go");
    assembly.call("RS", "go");

    // assembly.print_all();
}

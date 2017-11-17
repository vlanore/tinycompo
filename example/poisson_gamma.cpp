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

#include "poisson_gamma_connectors.hpp"

struct PoissonGamma : public Composite {
    static void contents(Model& model, int size) {
        model.component<Exponential>("Sigma").connect<Set>("paramConst", 1.0);

        model.component<Exponential>("Theta").connect<Set>("paramConst", 1.0);

        model.composite<Array<Gamma>>("Omega", size).connect<MultiProvide<Real>>("paramPtr", Address("Theta"));

        model.composite<Array<Product>>("rate", size)
            .connect<ArrayOneToOne<Real>>("aPtr", Address("Omega"))
            .connect<MultiProvide<Real>>("bPtr", Address("Sigma"));

        model.composite<Array<Poisson>>("X", size).connect<ArrayOneToOne<Real>>("paramPtr", Address("rate"));
    }
};

struct Moves : public Composite {
    static void contents(Model& model, int size) {
        model.component<MHMove<Scaling>>("MoveSigma", 3, 10).annotate("target", "Sigma");

        model.component<MHMove<Scaling>>("MoveTheta", 3, 10).annotate("target", "Theta");

        model.composite<Array<MHMove<Scaling>>>("MoveOmega", size, 3, 10).annotate("target", "Omega");

        model.component<GammaSuffStat>("GammaSuffStat").annotate("target", "Omega");
    }
};

int main() {
    srand(time(NULL));

    Model model;

    // graphical model part
    int size = 5;
    vector<double> data{0, 1, 1, 0, 1};
    model.composite<PoissonGamma>("PG", size);
    model.connect<ArraySet>(PortAddress("clamp", "PG", "X"), data);
    model.connect<ArraySet>(PortAddress("value", "PG", "X"), data);

    // MCMC infrastructure
    model.component<MultiSample>("sampler").connect<UseAllUnclampedNodes>("register", Address("PG"));

    model.component<MoveScheduler>("scheduler");
    model.composite<Moves>("moves", size);
    model.connect<ConnectAllMoves>(Address("moves"), Address("PG"), Address("scheduler"));

    model.component<FileOutput>("tracefile", "tmp_mcmc.trace");

    model.component<MCMCEngine>("MCMC", 10000)
        .connect<Use<Sampler>>("sampler", Address("sampler"))
        .connect<Use<MoveScheduler>>("scheduler", Address("scheduler"))
        .connect<ListUse<Real>>("variables", Address("PG", "Theta"), Address("PG", "Sigma"))
        .connect<Use<DataStream>>("output", Address("tracefile"));

    // RS infrastructure
    model.component<RejectionSampling>("RS", 500000)
        .connect<Use<Sampler>>("sampler", Address("sampler2"))
        .connect<MultiUse<RandomNode>>("data", Address("PG", "X"))
        .connect<Use<DataStream>>("output", Address("traceFile2"));

    model.component<MultiSample>("sampler2").connect<UseTopoSortInComposite<RandomNode>>("register", Address("PG"));

    model.component<FileOutput>("traceFile2", "tmp_rs.trace");

    // instantiate and call everything!
    Assembly assembly(model);
    assembly.call("MCMC", "go");
    assembly.call("RS", "go");

    // DEBUG
    Model(_Type<PoissonGamma>(), 3).dot_to_file("tmp_pg.dot");
    model.dot_to_file();
    // model.print();
    // assembly.print_all();
}

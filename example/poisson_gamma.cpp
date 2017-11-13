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
        model.component<MHMove<Scaling>>("Move_Sigma", 3, 10);
        model.meta("Move_Sigma", "target", "Sigma");

        model.component<MHMove<Scaling>>("Move_Theta", 3, 10);
        model.meta("Move_Theta", "target", "Theta");

        model.composite<Array<MHMove<Scaling>>>("Move_Omega", size, 3, 10);
        model.meta("Move_Omega", "target", "Omega");

        model.component<GammaSuffStat>("GammaSuffStat");
        model.meta("GammaSuffStat", "target", "Omega");
    }
};

int main() {
    srand(time(NULL));

    Model model;

    // graphical model part
    int size = 5;
    vector<double> data{0, 1, 1, 0, 1};
    auto pg = model.composite<PoissonGamma>("PG", size);
    model.connect<ArraySet>(PortAddress("clamp", "PG", "X"), data);
    model.connect<ArraySet>(PortAddress("value", "PG", "X"), data);

    // MCMC infrastructure
    auto sampler = model.component<MultiSample>("sampler");
    model.connect<UseAllUnclampedNodes>(PortAddress("register", sampler), pg);

    auto scheduler = model.component<MoveScheduler>("scheduler");
    auto moves = model.composite<Moves>("moves", size);
    model.connect<ConnectAllMoves>(moves, pg, scheduler);

    auto mcmc_engine = model.component<MCMCEngine>("MCMC", 10000);
    model.connect<Use<Sampler>>(PortAddress("sampler", mcmc_engine), sampler);
    model.connect<Use<MoveScheduler>>(PortAddress("scheduler", mcmc_engine), scheduler);
    model.connect<ListUse<Real>>(PortAddress("variables", mcmc_engine), Address(pg, "Theta"), Address(pg, "Sigma"));

    auto tracefile = model.component<FileOutput>("traceFile", "tmp_mcmc.trace");
    model.connect<Use<DataStream>>(PortAddress("output", mcmc_engine), tracefile);

    // RS infrastructure
    model.component<RejectionSampling>("RS", 500000);
    model.connect<Use<Sampler>>(PortAddress("sampler", "RS"), Address("sampler2"));
    model.connect<MultiUse<RandomNode>>(PortAddress("data", "RS"), Address("PG", "X"));

    model.component<MultiSample>("sampler2");
    model.connect<UseTopoSortInComposite<RandomNode>>(PortAddress("register", "sampler2"), pg);

    model.component<FileOutput>("traceFile2", "tmp_rs.trace");
    model.connect<Use<DataStream>>(PortAddress("output", "RS"), Address("traceFile2"));

    // instantiate and call everything!
    Assembly assembly(model);
    assembly.call(mcmc_engine, "go");
    assembly.call("RS", "go");

    // DEBUG
    Model(_Type<PoissonGamma>(), 3).dot_to_file("tmp_pg.dot");
    model.dot_to_file();
    // model.print();
    // assembly.print_all();
}

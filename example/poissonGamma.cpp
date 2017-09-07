/*
Copyright or © or Copr. Centre National de la Recherche Scientifique (CNRS) (2017/05/03)
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

#include "graphicalModel.hpp"

struct PoissonGamma : public Composite<> {
    PoissonGamma() {
        component<Exponential>("Sigma");
        connect<Set>(Address("Sigma"), "paramConst", 1.0);

        component<Exponential>("Theta");
        connect<Set>(Address("Theta"), "paramConst", 1.0);

        composite<Array<Gamma>>("Omega", 5);
        connect<MultiProvide<Real>>("Omega", "paramPtr", "Theta");

        composite<Array<Product>>("rate", 5);
        connect<ArrayOneToOne<Real>>("rate", "aPtr", "Omega");
        connect<MultiProvide<Real>>("rate", "bPtr", "Sigma");

        composite<Array<Poisson>>("X", 5);
        connect<ArrayOneToOne<Real>>("X", "paramPtr", "rate");
        connect<ArraySet>(Address("X"), "clamp", std::vector<double>{0, 1, 0, 0, 1});
    }
};

int main() {
    Model<> model;

    // graphical model part
    model.composite<PoissonGamma>("PG");

    // sampler
    model.component<MultiSample>("Sampler");
    model.connect<UseProvide<RandomNode>>(Address("Sampler"), "register", Address("PG", "Sigma"));
    model.connect<UseProvide<RandomNode>>(Address("Sampler"), "register", Address("PG", "Theta"));
    model.connect<MultiUse<RandomNode>>(Address("Sampler"), "register", Address("PG", "Omega"));
    model.connect<MultiUse<RandomNode>>(Address("Sampler"), "register", Address("PG", "X"));

    model.component<RejectionSampling>("RS", 10000);
    model.connect<UseProvide<Sampler>>(Address("RS"), "sampler", Address("Sampler"));
    model.connect<MultiUse<RandomNode>>(Address("RS"), "data", Address("PG", "X"));

    model.component<ConsoleOutput>("Console");
    model.connect<UseProvide<DataStream>>(Address("RS"), "output", Address("Console"));
    // model.component<FileOutput>("TraceFile", "tmp.trace");
    // model.connect<UseProvide<DataStream>>(Address("RS"), "output", Address("TraceFile"));

    model.debug();

    // instantiate everything!
    Assembly<> assembly(model);

    // call sampling
    assembly.call("RS", "go");

    assembly.print_all();
}

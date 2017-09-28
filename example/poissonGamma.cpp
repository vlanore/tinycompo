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
using namespace std;

void _iF(stringstream&) {}

template <class Thing, class... Things>
void _iF(stringstream& ss, Thing thing, Things... things) {
    ss << thing;
    _iF(ss, things...);
}

template <class... Things>
string iF(Things... things) {
    stringstream ss;
    _iF(ss, things...);

    return ss.str();
}

default_random_engine generator;
uniform_real_distribution<double> uniform{0.0, 1.0};

struct Uniform {
    static double move(RandomNode* v) {
        v->setValue(uniform(generator));
        return 1.;
    }
};

template <class Move>
class MHMove : public Go {
    RandomNode* node{nullptr};
    vector<RandomNode*> downward;
    void addDownward(RandomNode* ptr) { downward.push_back(ptr); }

  public:
    MHMove() {
        port("node", &MHMove::node);
        port("downward", &MHMove::addDownward);
    }

    void go() {
        double backup = node->getValue();

        auto gather = [](const vector<RandomNode*>& v) {
            return accumulate(v.begin(), v.end(), 1., [](double acc, RandomNode* b) { return acc * b->density(); });
        };
        double likelihood_before = gather(downward) * node->density();
        // std::cout << "Likelihood before: " << likelihood_before << '\n';

        double hastings_ratio = Move::move(node);

        double likelihood_after = gather(downward) * node->density();
        // std::cout << "Likelihood after: " << likelihood_after << '\n';

        bool accepted = (likelihood_after / likelihood_before * hastings_ratio) > uniform(generator);
        // std::cout << "Accepted :" << accepted << '\n';
        if (!accepted) {
            node->setValue(backup);
        }
        // return accepted;
    }
};

struct MoveScheduler : public Go {};

class DummyScheduler : public MoveScheduler {
    vector<Go*> move{};
    void addMove(Go* ptr) { move.push_back(ptr); }

  public:
    DummyScheduler() { port("move", &DummyScheduler::addMove); }

    void go() {
        for (auto ptr : move) {
            ptr->go();
        }
    }
};

class BayesianEngine : public Go {
    MoveScheduler* scheduler{nullptr};
    Sampler* sampler{nullptr};
    DataStream* output{nullptr};
    int iterations;

    vector<Real*> variables_of_interest{};
    void addVarOfInterest(Real* val) { variables_of_interest.push_back(val); }

  public:
    explicit BayesianEngine(int iterations = 10) : iterations(iterations) {
        port("variables", &BayesianEngine::addVarOfInterest);
        port("scheduler", &BayesianEngine::scheduler);
        port("sampler", &BayesianEngine::sampler);
        port("iterations", &BayesianEngine::iterations);
        port("output", &BayesianEngine::output);
    }

    void go() {
        sampler->go();
        output->header("# Theta\tSigma");
        for (int i = 0; i < iterations; i++) {
            scheduler->go();
            vector<double> vect;
            for (auto v : variables_of_interest) {
                vect.push_back(v->getValue());
            }
            output->dataLine(vect);
        }
    }
};

struct PoissonGamma : public Composite<> {
    PoissonGamma() {
        component<Exponential>("Sigma");
        connect<Set>(PortAddress("paramConst", "Sigma"), 1.0);

        component<Exponential>("Theta");
        connect<Set>(PortAddress("paramConst", "Theta"), 1.0);

        composite<Array<Gamma>>("Omega", 5);
        connect<MultiProvide<Real>>(PortAddress("paramPtr", "Omega"), Address("Theta"));

        composite<Array<Product>>("rate", 5);
        connect<ArrayOneToOne<Real>>(PortAddress("aPtr", "rate"), Address("Omega"));
        connect<MultiProvide<Real>>(PortAddress("bPtr", "rate"), Address("Sigma"));

        composite<Array<Poisson>>("X", 5);
        connect<ArrayOneToOne<Real>>(PortAddress("paramPtr", "X"), Address("rate"));
        connect<ArraySet>(PortAddress("clamp", "X"), vector<double>{0, 1, 0, 0, 1});
    }
};

int main() {
    Model<> model;

    // graphical model part
    model.composite<PoissonGamma>("PG");

    // sampler
    model.component<MultiSample>("Sampler");
    model.connect<ListUse<RandomNode>>(PortAddress("register", "Sampler"), Address("PG", "Sigma"), Address("PG", "Theta"));
    model.connect<MultiUse<RandomNode>>(PortAddress("register", "Sampler"), Address("PG", "Omega"));
    model.connect<MultiUse<RandomNode>>(PortAddress("register", "Sampler"), Address("PG", "X"));

    // model.component<RejectionSampling>("RS", 10000);
    // model.connect<Use<Sampler>>(PortAddress("sampler", "RS"), Address("Sampler"));
    // model.connect<MultiUse<RandomNode>>(PortAddress("data", "RS"), Address("PG", "X"));

    model.component<BayesianEngine>("BI", 50000);
    model.connect<Use<Sampler>>(PortAddress("sampler", "BI"), Address("Sampler"));
    model.connect<Use<MoveScheduler>>(PortAddress("scheduler", "BI"), Address("Scheduler"));
    model.connect<ListUse<Real>>(PortAddress("variables", "BI"), Address("PG", "Theta"), Address("PG", "Sigma"));

    model.component<DummyScheduler>("Scheduler");

    // Moves
    model.component<MHMove<Uniform>>("ThetaMove");
    model.connect<Use<RandomNode>>(PortAddress("node", "ThetaMove"), Address("PG", "Theta"));
    model.connect<Use<Go>>(PortAddress("move", "Scheduler"), Address("SigmaMove"));

    model.component<MHMove<Uniform>>("SigmaMove");
    model.connect<Use<RandomNode>>(PortAddress("node", "SigmaMove"), Address("PG", "Sigma"));
    model.connect<Use<Go>>(PortAddress("move", "Scheduler"), Address("SigmaMove"));
    for (int i = 0; i < 5; i++) {
        model.connect<Use<RandomNode>>(PortAddress("downward", "ThetaMove"), Address("PG", "Omega", i));
        model.connect<Use<RandomNode>>(PortAddress("downward", "SigmaMove"), Address("PG", "X", i));
        const char* moveName = iF("OmegaMove", i).c_str();
        model.component<MHMove<Uniform>>(moveName);
        model.connect<Use<RandomNode>>(PortAddress("node", moveName), Address("PG", "Omega", i));
        model.connect<Use<RandomNode>>(PortAddress("downward", moveName), Address("PG", "X", i));
        model.connect<Use<Go>>(PortAddress("move", "Scheduler"), Address(moveName));
    }

    // model.component<ConsoleOutput>("Console");
    // model.connect<Use<DataStream>>(PortAddress("output", "RS"), Address("Console"));
    model.component<FileOutput>("TraceFile", "tmp.trace");
    model.connect<Use<DataStream>>(PortAddress("output", "BI"), Address("TraceFile"));

    // PoissonGamma().dotToFile();
    model.dotToFile();

    // instantiate everything!
    Assembly<> assembly(model);

    // call sampling
    assembly.call("BI", "go");

    assembly.print_all();
}

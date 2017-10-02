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

#include <regex>
#include "graphicalModel.hpp"
using namespace std;

struct Uniform {
    static double move(RandomNode* v, double = 1.0) {
        v->setValue(uniform(generator));
        return 0.;
    }
};

struct Scaling {
    static double move(RandomNode* v, double tuning = 1.0) {
        auto multiplier = exp(tuning * (uniform(generator) - 0.5));
        // cout << multiplier << '\n';
        v->setValue(v->getValue() * multiplier);
        return 0.;
    }
};

template <class Move>
class MHMove : public Go {
    double tuning;
    int ntot{0}, nacc{0}, nrep{0};
    RandomNode* node{nullptr};
    vector<RandomNode*> downward;
    void addDownward(RandomNode* ptr) { downward.push_back(ptr); }

  public:
    explicit MHMove(double tuning = 1.0, int nrep = 1) : tuning(tuning), nrep(nrep) {
        port("node", &MHMove::node);
        port("downward", &MHMove::addDownward);
    }

    void go() override {
        for (int i = 0; i < nrep; i++) {
            double backup = node->getValue();

            auto gather = [](const vector<RandomNode*>& v) {
                return accumulate(v.begin(), v.end(), 0., [](double acc, RandomNode* b) { return acc + b->logDensity(); });
            };
            double likelihood_before = gather(downward) + node->logDensity();
            double hastings_ratio = Move::move(node, tuning);
            double likelihood_after = gather(downward) + node->logDensity();

            // cout << sf("Move: ll before = %.2f, ll after = %.2f\n", exp(likelihood_before), exp(likelihood_after));

            bool accepted = exp(likelihood_after - likelihood_before + hastings_ratio) > uniform(generator);
            if (!accepted) {
                node->setValue(backup);
            } else {
                nacc++;
            }
            ntot++;
        }
    }

    string _debug() const override { return sf("MHMove[%.1f\%]", nacc * 100. / ntot); }
};

class MoveScheduler : public Component {
    vector<Go*> move{};
    void addMove(Go* ptr) { move.push_back(ptr); }

  public:
    MoveScheduler() {
        port("go", &MoveScheduler::go);
        port("move", &MoveScheduler::addMove);
    }

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
        cout << "-- Starting MCMC chain!\n";
        sampler->go();
        sampler->go();
        output->header("#Theta\tSigma");
        for (int i = 0; i < iterations; i++) {
            scheduler->go();
            vector<double> vect;
            for (auto v : variables_of_interest) {
                vect.push_back(v->getValue());
            }
            output->dataLine(vect);
        }
        cout << "-- Done. Wrote " << iterations << " lines in trace file.\n";
    }
};

struct PoissonGamma : public Composite<> {
    explicit PoissonGamma(int size) {
        component<Exponential>("Sigma");
        connect<Set>(PortAddress("paramConst", "Sigma"), 1.0);

        component<Exponential>("Theta");
        connect<Set>(PortAddress("paramConst", "Theta"), 1.0);

        composite<Array<Gamma>>("Omega", size);
        connect<MultiProvide<Real>>(PortAddress("paramPtr", "Omega"), Address("Theta"));

        composite<Array<Product>>("rate", size);
        connect<ArrayOneToOne<Real>>(PortAddress("aPtr", "rate"), Address("Omega"));
        connect<MultiProvide<Real>>(PortAddress("bPtr", "rate"), Address("Sigma"));

        composite<Array<Poisson>>("X", size);
        connect<ArrayOneToOne<Real>>(PortAddress("paramPtr", "X"), Address("rate"));
    }
};

template <class Key>
void configMoves(Model<Key>& model, const std::string& modelName, const std::string& schedName, const std::string& spec) {
    regex e2("([a-zA-Z0-9]+)\\(([a-zA-Z0-9]+),\\s*([0-9]+\\.?[0-9]*),\\s*([0-9]+),\\s*([a-zA-Z0-9\\s]+)\\)");

    for (sregex_iterator it{spec.begin(), spec.end(), e2}; it != sregex_iterator{}; it++) {
        int nrep = stoi((*it)[4]);
        double tuning = stof((*it)[3]);
        string node = (*it)[2];
        string moveName = sf("%sMove", node.c_str());
        if ((*it)[1] == "Scaling") {
            model.template component<MHMove<Scaling>>(moveName, tuning, nrep);
        } else if ((*it)[1] == "Uniform") {
            model.template component<MHMove<Uniform>>(moveName, tuning, nrep);
        } else {
            std::cerr << "Unknown move " << (*it)[1] << "!\n";
            exit(1);
        }
        model.template connect<Use<RandomNode>>(PortAddress("node", moveName), Address(modelName, node));
        model.template connect<Use<Go>>(PortAddress("move", schedName), Address(moveName));
    }
}

int main() {
    Model<> model;

    // graphical model part
    int size = 5;
    vector<double> data{0, 1, 1, 0, 1};
    model.composite<PoissonGamma>("PG", size);
    model.connect<ArraySet>(PortAddress("clamp", "PG", "X"), data);
    model.connect<ArraySet>(PortAddress("value", "PG", "X"), data);

    // bayesian engine
    model.component<BayesianEngine>("BI", 10000);
    model.connect<Use<Sampler>>(PortAddress("sampler", "BI"), Address("Sampler"));
    model.connect<Use<MoveScheduler>>(PortAddress("scheduler", "BI"), Address("Scheduler"));
    model.connect<ListUse<Real>>(PortAddress("variables", "BI"), Address("PG", "Theta"), Address("PG", "Sigma"));

    // output
    model.component<FileOutput>("TraceFile", "tmp.trace");
    model.connect<Use<DataStream>>(PortAddress("output", "BI"), Address("TraceFile"));

    // sampler
    model.component<MultiSample>("Sampler");
    model.connect<ListUse<RandomNode>>(PortAddress("register", "Sampler"), Address("PG", "Sigma"), Address("PG", "Theta"));
    model.connect<MultiUse<RandomNode>>(PortAddress("register", "Sampler"), Address("PG", "Omega"));

    // moves
    model.component<MoveScheduler>("Scheduler");

    configMoves(model, "PG", "Scheduler", "Scaling(Theta, 3, 10, array Omega), Scaling(Sigma, 3, 10, array X)");

    // model.component<MHMove<Scaling>>("ThetaMove", 3, 10);
    // model.connect<Use<RandomNode>>(PortAddress("node", "ThetaMove"), Address("PG", "Theta"));
    // model.connect<Use<Go>>(PortAddress("move", "Scheduler"), Address("ThetaMove"));

    // model.component<MHMove<Scaling>>("SigmaMove", 3, 10);
    // model.connect<Use<RandomNode>>(PortAddress("node", "SigmaMove"), Address("PG", "Sigma"));
    // model.connect<Use<Go>>(PortAddress("move", "Scheduler"), Address("SigmaMove"));

    model.connect<MultiUse<RandomNode>>(PortAddress("downward", "ThetaMove"), Address("PG", "Omega"));
    model.connect<MultiUse<RandomNode>>(PortAddress("downward", "SigmaMove"), Address("PG", "X"));

    model.composite<Array<MHMove<Scaling>>>("OmegaMove", 5, 3, 10);
    model.connect<ArrayOneToOne<RandomNode>>(PortAddress("node", "OmegaMove"), Address("PG", "Omega"));
    model.connect<ArrayOneToOne<RandomNode>>(PortAddress("downward", "OmegaMove"), Address("PG", "X"));
    model.connect<MultiUse<Go>>(PortAddress("move", "Scheduler"), Address("OmegaMove"));

    // RS infrastructure
    model.component<RejectionSampling>("RS", 100000);
    model.connect<Use<Sampler>>(PortAddress("sampler", "RS"), Address("Sampler2"));
    model.connect<MultiUse<RandomNode>>(PortAddress("data", "RS"), Address("PG", "X"));

    model.component<MultiSample>("Sampler2");
    model.connect<ListUse<RandomNode>>(PortAddress("register", "Sampler2"), Address("PG", "Theta"), Address("PG", "Sigma"));
    model.connect<MultiUse<RandomNode>>(PortAddress("register", "Sampler2"), Address("PG", "Omega"));
    model.connect<MultiUse<RandomNode>>(PortAddress("register", "Sampler2"), Address("PG", "X"));

    model.component<FileOutput>("TraceFile2", "tmp2.trace");
    model.connect<Use<DataStream>>(PortAddress("output", "RS"), Address("TraceFile2"));

    // PoissonGamma(3).dotToFile();
    model.dotToFile();

    // instantiate everything!
    Assembly<> assembly(model);

    assembly.call("BI", "go");
    assembly.call("RS", "go");

    assembly.print_all();
}

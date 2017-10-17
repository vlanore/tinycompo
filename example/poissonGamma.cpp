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

#include <regex>
#include "graphicalModel.hpp"
using namespace std;

template <class Interface>
struct UseInComposite {
    template <class... Keys, class... Keys2, class Arg, class... Args>
    static void _connect(Assembly<>& assembly, _PortAddress<Keys...> user, _Address<Keys2...> composite, Arg arg,
                         Args... args) {
        using Key = typename _Key<Arg>::actual_type;
        vector<Key> keys{arg, args...};
        auto& compositeRef = assembly.at<Assembly<Key>>(composite);
        auto& userRef = assembly.at(user.address);
        for (auto k : keys) {
            auto& providerRef = compositeRef.at(k);
            auto providerArrayPtr = dynamic_cast<Assembly<int>*>(&providerRef);
            if (providerArrayPtr != nullptr) {  // component is an array
                for (unsigned int i = 0; i < providerArrayPtr->size(); i++) {
                    userRef.set(user.prop, &providerArrayPtr->at<Interface>(i));
                }
            } else {
                userRef.set(user.prop, dynamic_cast<Interface*>(&providerRef));
            }
        }
    }
};

bool isRandomNode(Component& ref) { return dynamic_cast<RandomNode*>(&ref) != nullptr; }
bool isUnclamped(Component& ref) {
    auto refRandom = dynamic_cast<RandomNode*>(&ref);
    return (refRandom != nullptr) && (!refRandom->is_clamped);
}

// struct UseAllRandomNodes {
//     template <class... Keys, class... Keys2>
//     static void _connect(Assembly<>& assembly, _PortAddress<Keys...> user, _Address<Keys2...> model) {
//         auto& modelRef = assembly.at<Assembly<string>>(model);
//         auto& userRef = assembly.at(user.address);
//         for (auto k : modelRef.all_keys()) {
//             cerr << "AllRandom : " << k << '\n';
//             auto& providerRef = modelRef.at(k);
//             auto providerArrayPtr = dynamic_cast<Assembly<int>*>(&providerRef);
//             if (providerArrayPtr != nullptr && isRandomNode(providerArrayPtr->at(0))) {
//                 cerr << "array\n";
//                 for (unsigned int i = 0; i < providerArrayPtr->size(); i++) {
//                     userRef.set(user.prop, &providerArrayPtr->at<RandomNode>(i));
//                 }
//             } else if (isRandomNode(providerRef)){
//                 userRef.set(user.prop, dynamic_cast<RandomNode*>(&providerRef));
//             }
//             cerr << "Done.\n";
//         }
//     }
// };

struct UseAllUnclampedNodes {
    template <class... Keys, class... Keys2>
    static void _connect(Assembly<>& assembly, _PortAddress<Keys...> user, _Address<Keys2...> model) {
        auto& modelRef = assembly.at<Assembly<string>>(model);
        auto& userRef = assembly.at(user.address);
        for (auto k : modelRef.all_keys()) {
            auto& providerRef = modelRef.at(k);
            auto providerArrayPtr = dynamic_cast<Assembly<int>*>(&providerRef);
            if (providerArrayPtr != nullptr && isUnclamped(providerArrayPtr->at(0))) {
                for (unsigned int i = 0; i < providerArrayPtr->size(); i++) {
                    userRef.set(user.prop, &providerArrayPtr->at<RandomNode>(i));
                }
            } else if (isUnclamped(providerRef)) {
                userRef.set(user.prop, dynamic_cast<RandomNode*>(&providerRef));
            }
        }
    }
};

struct Uniform {
    static double move(RandomNode* v, double = 1.0) {
        v->setValue(uniform(generator));
        return 0.;
    }
};

struct Scaling {
    static double move(RandomNode* v, double tuning = 1.0) {
        auto multiplier = tuning * (uniform(generator) - 0.5);
        // cout << multiplier << '\n';
        v->setValue(v->getValue() * exp(multiplier));
        return multiplier;
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
                return accumulate(v.begin(), v.end(), 0., [](double acc, RandomNode* b) { return acc + b->log_density(); });
            };
            double logprob_before = gather(downward) + node->log_density();
            double hastings_ratio = Move::move(node, tuning);
            double logprob_after = gather(downward) + node->log_density();

            bool accepted = exp(logprob_after - logprob_before + hastings_ratio) > uniform(generator);
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

class MCMCEngine : public Go {
    MoveScheduler* scheduler{nullptr};
    Sampler* sampler{nullptr};
    DataStream* output{nullptr};
    int iterations;

    vector<Real*> variables_of_interest{};
    void addVarOfInterest(Real* val) { variables_of_interest.push_back(val); }

  public:
    explicit MCMCEngine(int iterations = 10) : iterations(iterations) {
        port("variables", &MCMCEngine::addVarOfInterest);
        port("scheduler", &MCMCEngine::scheduler);
        port("sampler", &MCMCEngine::sampler);
        port("iterations", &MCMCEngine::iterations);
        port("output", &MCMCEngine::output);
    }

    void go() {
        cout << "-- Starting MCMC chain!\n";
        sampler->go();
        sampler->go();
        string header = accumulate(variables_of_interest.begin(), variables_of_interest.end(), string("#"),
                                   [](string acc, Real* v) { return acc + v->get_name() + "\t"; });
        output->header(header);
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

bool isArray(const string& s) {
    regex e("array\\s([a-zA-Z0-9]+)");
    smatch m;
    regex_match(s, m, e);
    return m.size() == 2;
}

string arrayName(const string& s) {
    regex e("array\\s([a-zA-Z0-9]+)");
    smatch m;
    regex_match(s, m, e);
    return m.size() == 2 ? m[1] : s;
}

template <class Key>
void configMoves(Model<Key>& model, const string& modelName, const string& schedName, const string& spec) {
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
            cerr << "Unknown move " << (*it)[1] << "!\n";
            exit(1);
        }
        model.template connect<Use<RandomNode>>(PortAddress("node", moveName), Address(modelName, node));
        model.template connect<Use<Go>>(PortAddress("move", schedName), Address(moveName));
        string downwardString = (*it)[5];
        string downwardName = arrayName(downwardString);
        if (isArray(downwardString)) {
            model.template connect<MultiUse<RandomNode>>(PortAddress("downward", moveName),
                                                         Address(modelName, downwardName));
        } else {
            model.template connect<Use<RandomNode>>(PortAddress("downward", moveName), Address(modelName, downwardName));
        }
    }
}

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

// struct Moves : public Composite<> {
//     Moves() {
//         component<MHMove<Scaling>>("MoveSigma", 3, 10);
//         component<MHMove<Scaling>>("Theta", 3, 10);
//     }
// };

int main() {
    Model<> model;

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

    configMoves(model, "PG", "Scheduler", "Scaling(Theta, 3, 10, array Omega), Scaling(Sigma, 3, 10, array X)");

    model.composite<Array<MHMove<Scaling>>>("OmegaMove", 5, 3, 10);
    model.connect<ArrayOneToOne<RandomNode>>(PortAddress("node", "OmegaMove"), Address("PG", "Omega"));
    model.connect<ArrayOneToOne<RandomNode>>(PortAddress("downward", "OmegaMove"), Address("PG", "X"));
    model.connect<MultiUse<Go>>(PortAddress("move", "Scheduler"), Address("OmegaMove"));

    // RS infrastructure
    model.component<RejectionSampling>("RS", 500000);
    model.connect<Use<Sampler>>(PortAddress("sampler", "RS"), Address("Sampler2"));
    model.connect<MultiUse<RandomNode>>(PortAddress("data", "RS"), Address("PG", "X"));

    model.component<MultiSample>("Sampler2");
    model.connect<UseInComposite<RandomNode>>(PortAddress("register", "Sampler2"), Address("PG"), "Sigma", "Theta", "Omega",
                                              "X");

    model.component<FileOutput>("TraceFile2", "tmp_rs.trace");
    model.connect<Use<DataStream>>(PortAddress("output", "RS"), Address("TraceFile2"));

    PoissonGamma(3).dot_to_file("tmp_pg.dot");
    model.dot_to_file();

    // instantiate everything!
    Assembly<> assembly(model);

    assembly.call("MCMC", "go");
    assembly.call("RS", "go");

    assembly.print_all();
}

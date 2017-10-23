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

#ifndef GRAPHICAL_MODEL
#define GRAPHICAL_MODEL

#include <algorithm>
#include <fstream>
#include <functional>
#include <random>
#include "tinycompo.hpp"

using namespace std;

int factorial(int n) {
    if (n < 2) {
        return 1;
    } else {
        return n * factorial(n - 1);
    }
}

template <typename... Args>
std::string sf(const std::string &format, Args... args) {
    size_t size = snprintf(nullptr, 0, format.c_str(), args...) + 1;
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1);
}

std::random_device r;
std::default_random_engine generator(r());
std::uniform_real_distribution<double> uniform{0.0, 1.0};

/*
===================================================================================================
  INTERFACES
=================================================================================================*/
class Go : public Component {
  public:
    Go() { port("go", &Go::go); }
    virtual void go() = 0;
};

class Sampler : public Go {
  public:
    virtual std::vector<double> getSample() const = 0;
    virtual std::string getVarList() const = 0;
};

class Real : public Component {
  public:
    virtual double getValue() const = 0;
    virtual void setValue(double value) = 0;
};

class RandomNode : public Real {
    double clampedVal{0.0};

  public:
    RandomNode() = default;
    virtual void sample() = 0;
    void clamp(double val) {
        is_clamped = true;
        clampedVal = val;
    }
    double clamped_value() const { return clampedVal; }
    virtual bool is_consistent() const { return clampedVal == getValue(); }
    virtual double log_density() = 0;

    bool is_clamped{false};
};

class DataStream {
  public:
    virtual void header(const std::string &str) = 0;
    virtual void dataLine(const std::vector<double> &line) = 0;
};

/*
===================================================================================================
  Helper classes
=================================================================================================*/
class ConsoleOutput : public Component, public DataStream {
  public:
    std::string _debug() const override { return "ConsoleOutput"; }
    void header(const std::string &str) override { std::cout << str << std::endl; }
    void dataLine(const std::vector<double> &line) override {
        std::for_each(line.begin(), line.end(), [](double e) { std::cout << e << "  "; });
        std::cout << std::endl;
    }
};

class FileOutput : public Component, public DataStream {
    std::ofstream file{};
    std::string filename;

  public:
    explicit FileOutput(const std::string &filename) : filename(filename) { file.open(filename); }
    ~FileOutput() { file.close(); }
    std::string _debug() const override { return sf("FileOutput(%s)", filename.c_str()); }
    void header(const std::string &str) override { file << str << std::endl; }
    void dataLine(const std::vector<double> &line) override {
        std::for_each(line.begin(), line.end(), [this](double e) { file << e << "\t"; });
        file << std::endl;
    }
};

// little object to encapsulate having a constant OR a pointer to Real
class RealProp {
    int mode{0};  // 0:unset, 1:constant, 2:pointer
    Real *ptr{nullptr};
    double value{0};

  public:
    RealProp() = default;
    explicit RealProp(double value) : mode(1), value(value) {}
    explicit RealProp(Real *ptr) : mode(2), ptr(ptr) {}
    double getValue() const {
        if (mode == 1) {
            return value;
        } else if (mode == 2) {
            return ptr->getValue();
        } else {
            std::cerr << "Property is no set!\n";
            exit(1);
        }
    }
};

/*
===================================================================================================
  Graphical model nodes
=================================================================================================*/
class UnaryReal : public RandomNode {
  protected:
    RealProp param{};
    template <class... Args>
    void setParam(Args... args) {
        param = RealProp(std::forward<Args>(args)...);
    }

    double value{0.0};
    std::string name{};

  public:
    UnaryReal() = delete;
    explicit UnaryReal(const std::string &name) : name(name) {
        port("paramConst", &UnaryReal::setParam<double>);
        port("paramPtr", &UnaryReal::setParam<Real *>);
        port("sample", &UnaryReal::sample);
        port("clamp", &UnaryReal::clamp);
        port("value", &UnaryReal::setValue);
    };

    std::string _debug() const override {
        std::stringstream ss;
        ss << name << "(" << param.getValue() << "):" << value << "[" << clamped_value() << "]";
        return ss.str();
    }

    double getValue() const override { return value; }
    void setValue(double valuein) override { value = valuein; }
};

class Exponential : public UnaryReal {
  public:
    explicit Exponential(double value = 0.0) : UnaryReal("Exponential") { setValue(value); }

    void sample() override {
        std::exponential_distribution<> d(param.getValue());
        setValue(d(generator));
    }

    double log_density() final {
        auto lambda = param.getValue();
        auto x = getValue();
        return log(lambda) - x * lambda;
    }
};

class Gamma : public UnaryReal {
  public:
    explicit Gamma() : UnaryReal("Gamma") {}

    void sample() override {
        std::gamma_distribution<double> d{param.getValue(), param.getValue()};
        setValue(d(generator));
    }

    double log_density() final {
        auto alpha = param.getValue();
        auto beta = alpha;
        auto x = getValue();
        return (alpha - 1) * log(x) - log(tgamma(alpha)) - alpha * log(beta) - x / beta;
    }
};

class Poisson : public UnaryReal {
  public:
    explicit Poisson(double value = 0.0) : UnaryReal("Poisson") { setValue(value); }

    void sample() override {
        std::poisson_distribution<> d(param.getValue());
        setValue(d(generator));
    }

    double log_density() final {
        double k = getValue(), lambda = param.getValue();
        return k * log(lambda) - lambda - log(factorial(k));
    }
};

template <class Op>
class BinaryOperation : public Real {
    RealProp a{};
    RealProp b{};

  public:
    BinaryOperation() {
        port("aPtr", &BinaryOperation::setA<Real *>);
        port("bPtr", &BinaryOperation::setB<Real *>);
        port("bConst", &BinaryOperation::setB<double>);
    }

    template <class... Args>
    void setA(Args... args) {
        a = RealProp(std::forward<Args>(args)...);
    }

    template <class... Args>
    void setB(Args... args) {
        b = RealProp(std::forward<Args>(args)...);
    }

    void setA(Real *ptr) { a = RealProp(ptr); }
    double getValue() const override { return Op()(a.getValue(), b.getValue()); }
    void setValue(double) override { std::cerr << "-- Warning! Trying to set a deterministic node!\n"; }
    std::string _debug() const override {
        std::stringstream ss;
        ss << "BinaryOperation(" << a.getValue() << "," << b.getValue() << "):" << getValue();
        return ss.str();
    }
};

using Product = BinaryOperation<std::multiplies<double>>;

/*
===================================================================================================
  Sampling and RS
=================================================================================================*/
class MultiSample : public Sampler {
    std::vector<RandomNode *> nodes;
    void registerNode(RandomNode *ptr) { nodes.push_back(ptr); }

  public:
    MultiSample() { port("register", &MultiSample::registerNode); }

    void go() override {
        std::for_each(nodes.begin(), nodes.end(), [](RandomNode *n) { n->sample(); });
    }

    std::string _debug() const override { return "MultiSample"; }

    std::string getVarList() const override {
        return std::accumulate(nodes.begin(), nodes.end(), std::string("#"),
                               [](std::string acc, RandomNode *n) { return acc + n->get_name() + '\t'; });
    }

    std::vector<double> getSample() const override {
        std::vector<double> tmp(nodes.size(), 0.);
        std::transform(nodes.begin(), nodes.end(), tmp.begin(), [](RandomNode *n) { return n->getValue(); });
        return tmp;
    }
};

class RejectionSampling : public Go {
    std::vector<RandomNode *> observedData;
    void addData(RandomNode *ptr) { observedData.push_back(ptr); }

    Sampler *sampler{nullptr};
    int nbIter{0};
    DataStream *output{nullptr};

  public:
    explicit RejectionSampling(int iter = 5) : nbIter(iter) {
        port("sampler", &RejectionSampling::sampler);
        port("data", &RejectionSampling::addData);
        port("output", &RejectionSampling::output);
    }

    std::string _debug() const override { return "RejectionSampling"; }
    void go() override {
        int accepted = 0;
        std::cout << "-- Starting rejection sampling!\n";

        output->header(sampler->getVarList());
        for (auto i = 0; i < nbIter; i++) {
            sampler->go();
            bool valid = std::accumulate(observedData.begin(), observedData.end(), true,
                                         [](bool acc, RandomNode *n) { return acc && n->is_consistent(); });
            if (valid) {  // accept
                accepted++;
                output->dataLine(sampler->getSample());
            }
        }
        std::cout << "-- Done. Accepted " << accepted << " points.\n\n";
    }
};

/*
===================================================================================================
  MCMC-related things
=================================================================================================*/
struct Uniform {
    static double move(RandomNode *v, double = 1.0) {
        v->setValue(uniform(generator));
        return 0.;
    }
};

struct Scaling {
    static double move(RandomNode *v, double tuning = 1.0) {
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
    RandomNode *node{nullptr};
    vector<RandomNode *> downward;
    void addDownward(RandomNode *ptr) { downward.push_back(ptr); }

  public:
    explicit MHMove(double tuning = 1.0, int nrep = 1) : tuning(tuning), nrep(nrep) {
        port("node", &MHMove::node);
        port("downward", &MHMove::addDownward);
    }

    void go() override {
        for (int i = 0; i < nrep; i++) {
            double backup = node->getValue();

            auto gather = [](const vector<RandomNode *> &v) {
                return accumulate(v.begin(), v.end(), 0., [](double acc, RandomNode *b) { return acc + b->log_density(); });
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
    vector<Go *> move{};
    void addMove(Go *ptr) { move.push_back(ptr); }

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
    MoveScheduler *scheduler{nullptr};
    Sampler *sampler{nullptr};
    DataStream *output{nullptr};
    int iterations;

    vector<Real *> variables_of_interest{};
    void addVarOfInterest(Real *val) { variables_of_interest.push_back(val); }

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
                                   [](string acc, Real *v) { return acc + v->get_name() + "\t"; });
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

#endif

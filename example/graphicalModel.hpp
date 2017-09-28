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

#include <fstream>
#include <random>
#include "tinycompo.hpp"

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
};

class Real {
  public:
    virtual double getValue() const = 0;
    virtual void setValue(double value) = 0;
};

class RandomNode : public Real {
    double clampedVal{0.0};

  public:
    RandomNode() = default;
    virtual void sample() = 0;
    void clamp(double val) { clampedVal = val; }
    double clampedValue() const { return clampedVal; }
    virtual bool isConsistent() const { return clampedVal == getValue(); }
    virtual double logDensity() = 0;
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
        for (auto e : line) {
            std::cout << e << "  ";
        }
        std::cout << std::endl;
    }
};

class FileOutput : public Component, public DataStream {
    std::ofstream file{};

  public:
    explicit FileOutput(const std::string &filename) { file.open(filename); }
    ~FileOutput() { file.close(); }
    std::string _debug() const override { return "FileOutput"; }
    void header(const std::string &str) override { file << str << std::endl; }
    void dataLine(const std::vector<double> &line) override {
        for (auto e : line) {
            file << e << "\t";
        }
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
class UnaryReal : public Component, public RandomNode {
  protected:
    RealProp param{};
    double value{0.0};

  public:
    std::string name{};

    // constructors
    UnaryReal() = delete;
    explicit UnaryReal(const std::string &name) : name(name) {
        port("paramConst", &UnaryReal::setParam<double>);
        port("paramPtr", &UnaryReal::setParam<Real *>);
        port("sample", &UnaryReal::sample);
        port("clamp", &UnaryReal::clamp);
    };

    // methods required from parent classes
    std::string _debug() const override {
        std::stringstream ss;
        ss << name << "(" << param.getValue() << "):" << value << "[" << clampedValue() << "]";
        return ss.str();
    }
    double getValue() const override { return value; }
    void setValue(double valuein) override { value = valuein; }

    // class-specific methods
    template <class... Args>
    void setParam(Args... args) {
        param = RealProp(std::forward<Args>(args)...);
    }
};

class Exponential : public UnaryReal {
  public:
    explicit Exponential(double value = 0.0) : UnaryReal("Exponential") { setValue(value); }

    void sample() override {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::exponential_distribution<> d(param.getValue());
        setValue(d(gen));
    }

    double logDensity() final {
        double lambda = param.getValue();
        return log(lambda) - param.getValue() * lambda;
    }
};

class Gamma : public UnaryReal {
  public:
    explicit Gamma() : UnaryReal("Gamma") {}

    void sample() override {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::gamma_distribution<> d(param.getValue(), param.getValue());
        setValue(d(gen));
    }

    double logDensity() final {
        double k = param.getValue(), x = getValue();
        return (k - 1) * log(x) - x / k - k * log(k) + log(tgamma(k));
    }
};

class Poisson : public UnaryReal {
  public:
    explicit Poisson(double value = 0.0) : UnaryReal("Poisson") { setValue(value); }

    void sample() override {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::poisson_distribution<> d(param.getValue());
        setValue(d(gen));
    }

    double logDensity() final {
        double k = getValue(), lambda = param.getValue();
        std::function<int(int)> factorial = [&factorial](int n) {
            if (n < 2)
                return 1;
            else
                return n * factorial(n - 1);
        };
        return -lambda + k * log(lambda) - log(factorial(k));
    }
};

class Product : public Component, public Real {
    RealProp a{};
    RealProp b{};

  public:
    Product() {
        port("aPtr", &Product::setA<Real *>);
        port("bPtr", &Product::setB<Real *>);
        port("bConst", &Product::setB<double>);
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
    double getValue() const override { return a.getValue() * b.getValue(); }
    void setValue(double) override { std::cerr << "-- Warning! Trying to set a deterministic node!\n"; }
    std::string _debug() const override {
        std::stringstream ss;
        ss << "Product(" << a.getValue() << "," << b.getValue() << "):" << getValue();
        return ss.str();
    }
};

/*

===================================================================================================
  Moves and MCMC-related things
=================================================================================================*/
class SimpleMove : public Go {
    RandomNode *target{nullptr};

  public:
    SimpleMove() { port("target", &SimpleMove::setTarget); }

    std::string _debug() const override { return "SimpleMove"; }
    void go() override { target->sample(); }

    void setTarget(RandomNode *targetin) { target = targetin; }
};

class Scheduler : public Go {
    std::vector<SimpleMove *> moves;

  public:
    Scheduler() { port("register", &Scheduler::registerMove); }

    void go() override {
        std::cout << "\n-- Scheduler started!\n"
                  << "-- Sampling everything!\n";
        for (auto &move : moves) {
            move->go();
        }
        std::cout << "-- Done.\n\n";
    }

    std::string _debug() const override {
        std::stringstream ss;
        ss << "Scheduler[" << moves.size() << "]";
        return ss.str();
    }

    void registerMove(SimpleMove *ptr) { moves.push_back(ptr); }
};

class MultiSample : public Sampler {
    std::vector<RandomNode *> nodes;

  public:
    MultiSample() {
        port("register", &MultiSample::registerNode);
        port("registerVec", &MultiSample::registerVec);
    }

    void go() override {
        for (auto &node : nodes) {
            node->sample();
        }
    }

    std::string _debug() const override { return "MultiSample"; }

    void registerNode(RandomNode *ptr) { nodes.push_back(ptr); }

    void registerVec(std::vector<RandomNode *> vec) { nodes = vec; }

    std::vector<double> getSample() const override {
        std::vector<double> tmp{};
        for (auto node : nodes) {
            tmp.push_back(node->getValue());
        }
        return tmp;
    }
};

class RejectionSampling : public Go {
    std::vector<RandomNode *> observedData;
    Sampler *sampler{nullptr};
    int nbIter{0};
    DataStream *output{nullptr};

  public:
    explicit RejectionSampling(int iter = 5) {
        nbIter = iter;
        port("sampler", &RejectionSampling::sampler);
        port("data", &RejectionSampling::addData);
        port("output", &RejectionSampling::output);
    }

    void addData(RandomNode *ptr) { observedData.push_back(ptr); }

    std::string _debug() const override { return "RejectionSampling"; }
    void go() override {
        int accepted = 0;
        std::cout << "-- Starting rejection sampling!\n";
        std::stringstream ss;
        ss << "# Rejection sampling with " << nbIter << " iterations.";
        output->header(ss.str());
        for (auto i = 0; i < nbIter; i++) {
            // std::cout << "\t* Iteration " << i << ". Sampling!\n";
            sampler->go();
            // std::cout << "\t* Sampling done. Checking validity.\n";
            bool valid = true;
            for (auto node : observedData) {
                valid = valid && node->isConsistent();
            }
            if (valid) {  // accept
                accepted++;
                output->dataLine(sampler->getSample());
                // std::cout << "\t* Sample is valid!\n";
            } else {  // reject
                // std::cout << "\t* Sample is not valid :(\n";
            }
        }
        std::cout << "-- Done. Accepted " << accepted << " points.\n\n";
    }
};

/*

// ===================================================================================================
//   Plates and custom connectors
// =================================================================================================*/
// // GraphicalModel is a specialized Assembly which provides graphical-model-specific functionalities
// // such as giving a way to differentiate between random and deterministic nodes.
// class GraphicalModel : public Composite<> {
//   public:
//     std::vector<RandomNode *> all_random_nodes() const {
//         std::vector<RandomNode *> result;
//         for (auto &node : instances) {
//             auto ptr = dynamic_cast<RandomNode *>(node.second.get());
//             if (ptr != nullptr) {  // node is a lone RandomNode
//                 result.push_back(ptr);
//             } else {  // try to see if it's an array
//                 auto ptr2 = dynamic_cast<Assembly<int> *>(node.second.get());
//                 if (ptr2 != nullptr) {
//                     // std::cout << "Found an array\n";
//                     auto ptr3 = dynamic_cast<RandomNode *>(&ptr2->at(0));
//                     if (ptr3 != nullptr) {
//                         for (int i = 0; i < static_cast<int>(ptr2->size()); i++) {
//                             result.push_back(&ptr2->at<RandomNode>(i));
//                         }
//                     }
//                 }
//             }
//         }
//         // std:: cout << "SIZE OF VECTOR : " << result.size() << std::endl;
//         return result;
//     }
// };

// class UseAllRandomNodes {
//   public:
//     static void _connect(Assembly<> &a, const std::string &user, const std::string &prop,
//                          const std::string &gm) {
//         auto &userRef = a.at(user);
//         std::vector<RandomNode *> nodes = a.at<GraphicalModel>(gm).all_random_nodes();
//         userRef.set(prop, nodes);
//     }
// };

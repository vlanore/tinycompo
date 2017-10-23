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

#ifndef POISSON_GAMMA_CONNECTORS
#define POISSON_GAMMA_CONNECTORS

#include <regex>
#include "graphical_model.hpp"

using namespace std;

template <class Interface>
struct UseInComposite {
    template <class... Args>
    static void _connect(Assembly& assembly, PortAddress user, Address composite, Args... args) {
        for (auto k : vector<string>{args...}) {
            Address provider(composite, k);
            if (assembly.is_composite(provider)) {
                for (unsigned i = 0; i < assembly.at<Assembly>(provider).size(); i++) {
                    assembly.at(user.address).set(user.prop, &assembly.at<Interface>(Address(provider, i)));
                }
            } else {
                assembly.at(user.address).set(user.prop, &assembly.at<Interface>(provider));
            }
        }
    }
};

bool isRandomNode(Component& ref) { return dynamic_cast<RandomNode*>(&ref) != nullptr; }
bool isUnclamped(Component& ref) {
    auto refRandom = dynamic_cast<RandomNode*>(&ref);
    return (refRandom != nullptr) && (!refRandom->is_clamped);
}

struct UseAllUnclampedNodes {
    static void _connect(Assembly& assembly, PortAddress user, Address model) {
        auto& modelRef = assembly.at<Assembly>(model);
        auto& userRef = assembly.at(user.address);
        for (auto k : modelRef.all_keys()) {
            auto& providerRef = modelRef.at(k);
            auto providerArrayPtr = dynamic_cast<Assembly*>(&providerRef);
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

void configMoves(Model& model, const string& modelName, const string& schedName, const string& spec) {
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

template <class Interface>
struct AdaptiveUse {
    static void _connect(Assembly& assembly, PortAddress user, Address provider) {
        bool user_is_array = assembly.derives_from<Assembly>(user.address);
        bool provider_is_array = assembly.derives_from<Assembly>(provider);
        if (!user_is_array and !provider_is_array and assembly.derives_from<Interface>(provider)) {
            Use<Interface>::_connect(assembly, user, provider);
        } else if (!user_is_array and provider_is_array and assembly.derives_from<Interface>(Address(provider, 0))) {
            MultiUse<Interface>::_connect(assembly, user, provider);
        } else if (user_is_array and !provider_is_array and assembly.derives_from<Interface>(provider)) {
            MultiProvide<Interface>::_connect(assembly, user, provider);
        } else if (user_is_array and provider_is_array and assembly.derives_from<Interface>(Address(provider, 0))) {
            ArrayOneToOne<Interface>::_connect(assembly, user, provider);
        }
    }
};

template <class Interface>
struct UseTopoSortInComposite {
    static void _connect(Assembly& assembly, PortAddress user, Address composite) {
        // get graphical representation object
        const _AssemblyGraph graph = assembly.at<Assembly>(composite).get_model().get_representation();

        // gather edges and node names
        set<string> nodes;
        multimap<string, string> edges;
        for (auto c : graph.connectors) {
            // if connector is of the form (PortAddress, Address)
            if ((c.neighbors.size() == 2) and (c.neighbors[0].port != "") and (c.neighbors[1].port == "")) {
                edges.insert(make_pair(c.neighbors[0].address, c.neighbors[1].address));
                nodes.insert(c.neighbors[0].address);
                nodes.insert(c.neighbors[1].address);
            }
        }

        // gather nodes without predecessors
        set<string> starting_nodes;
        auto gather_nodes = [&]() {
            for (auto n : nodes)
                if (edges.count(n) == 0) starting_nodes.insert(n);
        };
        gather_nodes();

        // do the topological sorting!
        auto erase_edges_from = [&](string name) {
            for (auto it = edges.begin(); it != edges.end();) {
                if ((*it).second == name)
                    it = edges.erase(it);
                else
                    ++it;
            }
        };
        vector<string> sorted;
        while (starting_nodes.size() != 0) {
            string node = *starting_nodes.begin();
            sorted.push_back(node);
            starting_nodes.erase(node);
            nodes.erase(node);
            erase_edges_from(node);
            gather_nodes();
        }

        // connect!
        for (auto n : sorted) {
            AdaptiveUse<Interface>::_connect(assembly, user, Address(composite, n));
        }
    }
};

#endif

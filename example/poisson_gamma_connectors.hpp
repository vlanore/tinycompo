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

#ifndef POISSON_GAMMA_CONNECTORS
#define POISSON_GAMMA_CONNECTORS

#include <regex>
#include "graphical_model.hpp"

using namespace std;

template <class Interface>
struct AdaptiveUse {
    static void _connect(Assembly& assembly, PortAddress user, Address provider) {
        bool user_is_array = assembly.is_composite(user.address);
        bool provider_is_array = assembly.is_composite(provider);
        if (!user_is_array and !provider_is_array and assembly.derives_from<Interface>(provider)) {
            Use<Interface>::_connect(assembly, user, provider);
        } else if (!user_is_array and provider_is_array and assembly.derives_from<Interface>(Address(provider, 0))) {
            MultiUse<Interface>::_connect(assembly, user, provider);
        } else if (user_is_array and !provider_is_array and assembly.derives_from<Interface>(provider)) {
            MultiProvide<Interface>::_connect(assembly, user, provider);
        } else if (user_is_array and provider_is_array and assembly.derives_from<Interface>(Address(provider, 0))) {
            ArrayOneToOne<Interface>::_connect(assembly, user, provider);
        }  // else do nothing (TODO: maybe add a warning?)
    }
};

// template <class Interface>
// struct UseInComposite {
//     template <class... Args>
//     static void _connect(Assembly& assembly, PortAddress user, Address composite, Args... args) {
//         for (auto k : vector<string>{args...}) {
//             Address provider(composite, k);
//             AdaptiveUse<Interface>::_connect(assembly, user, provider);
//         }
//     }
// };

struct UseAllUnclampedNodes {
    static void _connect(Assembly& assembly, PortAddress user, Address model) {
        for (auto n : assembly.at<Assembly>(model).get_model().all_component_names(1)) {
            Address provider(model, address_from_composite_string(n));
            bool is_random_node = assembly.derives_from<RandomNode>(provider);
            bool is_not_clamped = is_random_node and !assembly.at<RandomNode>(provider).is_clamped;
            if (is_not_clamped) {
                AdaptiveUse<RandomNode>::_connect(assembly, user, provider);
            }
        }
    }
};

template <class Interface>
struct UseTopoSortInComposite {
    static void _connect(Assembly& assembly, PortAddress user, Address composite) {
        // get graphical representation object
        auto graph = assembly.at<Assembly>(composite).get_model().get_digraph();
        auto nodes = graph.first;
        auto edges = graph.second;

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

struct MarkovBlanket {  // assumes nodes have access to their parents (thus, blanket is just children of target)
    static void _connect(Assembly& assembly, PortAddress user, Address model, const string& target) {
        auto graph = assembly.at<Assembly>(model).get_model().get_digraph();

        vector<string> blanket;
        std::function<void(const string&)> find_blanket = [&](const string name) {
            for (auto e : graph.second) {  // graph.second is edges
                if (e.second == name) {
                    // if child is RandomNode then add it to blanket
                    Address origin = Address(model, e.first);
                    bool origin_is_random = assembly.is_composite(origin)
                                                ? assembly.derives_from<RandomNode>(Address(origin, 0))
                                                : assembly.derives_from<RandomNode>(origin);
                    if (origin_is_random) {
                        blanket.push_back(e.first);
                    } else {  // else, keep going
                        find_blanket(e.first);
                    }
                }
            }
        };
        find_blanket(target);

        for (auto n : blanket) {
            AdaptiveUse<LogDensity>::_connect(assembly, user, Address(model, n));
        }
    }
};

struct ConnectMove {
    static void _connect(Assembly& assembly, Address move, Address model, const string& target, Address scheduler) {
        AdaptiveUse<Go>::_connect(assembly, PortAddress("move", scheduler), move);  // connect scheduler to move
        AdaptiveUse<RandomNode>::_connect(assembly, PortAddress("node", move), Address(model, target));
        MarkovBlanket::_connect(assembly, PortAddress("downward", move), model, target);
    }
};

struct ConnectAllMoves {
    static string strip(string s) {
        auto it = s.find('_');
        return s.substr(++it);
    }

    static void _connect(Assembly& assembly, Address moves, Address model, Address suffstats, Address scheduler) {
        vector<string> move_names = assembly.at<Assembly>(moves).get_model().all_component_names(0, true);
        vector<string> suffstat_names = assembly.at<Assembly>(suffstats).get_model().all_component_names(0);
        auto global_graph = assembly.get_model().get_digraph();

        for (auto m : move_names) {
            auto m_address = Address(moves, m);
            auto m_target = Address(model, strip(m));

            // is there a suffstat for the target?
            for (auto s : suffstat_names) {  // for all suffstats
            }

            bool is_move = assembly.is_composite(m_address) ? assembly.derives_from<Move>(Address(m_address, 0))
                                                            : assembly.derives_from<Move>(m_address);
            if (assembly.derives_from<AbstractSuffStats>(m_address)) {  // sufftstats
                // searching for parent (assuming single) of target (FIXME temporary)
                Address m_parent("invalid");  // no default constructor
                auto graph = assembly.at<Assembly>(model).get_model().get_digraph();
                for (auto e : graph.second) {
                    if (e.first == strip(m)) {
                        m_parent = Address(model, e.second);
                    }
                }
                AdaptiveUse<RandomNode>::_connect(assembly, PortAddress("parent", m_address), m_parent);
                AdaptiveUse<RandomNode>::_connect(assembly, PortAddress("target", m_address), m_target);
            } else if (is_move) {  // moves
                ConnectMove::_connect(assembly, m_address, model, strip(m), scheduler);
            }
        }
    }
};

#endif

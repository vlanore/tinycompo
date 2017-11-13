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

#include <algorithm>
#include <iostream>
#include <sstream>
#include <tinycompo.hpp>
#include <vector>

using namespace tc;

class TextProcessor {
  public:
    virtual std::string process(const std::string& in) const = 0;
};

class TextSource {
  public:
    virtual std::string get() const = 0;
};

class ReplaceChar : public TextProcessor, public Component {
    char from, to;

  public:
    ReplaceChar(char from, char to) : from(from), to(to) {}

    std::string _debug() const { return "ReplaceChar"; }

    std::string process(const std::string& in) const {
        std::string result = in;
        std::replace(result.begin(), result.end(), from, to);
        return result;
    }
};

class ConstantText : public Component, public TextSource {
    std::string text;

  public:
    explicit ConstantText(const std::string& in) : text(in){};

    std::string _debug() const {
        std::stringstream ss;
        ss << "ConstantText: " << text;
        return ss.str();
    }

    std::string get() const { return text; }
};

class ProcessAndPrint : public Component {
    std::vector<TextProcessor*> effects;
    TextSource* source = nullptr;

  public:
    ProcessAndPrint() {
        port("effect", &ProcessAndPrint::setEffect);
        port("source", &ProcessAndPrint::setSource);
        port("go", &ProcessAndPrint::go);
    }

    void setEffect(TextProcessor* effectin) { effects.push_back(effectin); }
    void setSource(TextSource* sourcein) { source = sourcein; }

    std::string _debug() const { return "ProcessAndPrint"; }

    void go() {
        std::string text = source->get();
        for (auto ptr : effects) {
            text = ptr->process(text);
        }
        std::cout << text;
    }
};

int main() {
    Model mymodel;
    mymodel.component<ConstantText>("MyText", "Hello, I'm a rabbit.\nI like carrots.\n");
    mymodel.component<ReplaceChar>("ReplaceAbyB", 'a', 'b');
    mymodel.component<ReplaceChar>("ReplaceBbyD", 'b', 'd');
    mymodel.component<ProcessAndPrint>("Controller");
    mymodel.connect<Use<TextProcessor>>(PortAddress("effect", "Controller"), Address("ReplaceAbyB"));
    mymodel.connect<Use<TextProcessor>>(PortAddress("effect", "Controller"), Address("ReplaceBbyD"));
    mymodel.connect<Use<TextSource>>(PortAddress("source", "Controller"), Address("MyText"));

    Assembly myassembly(mymodel);
    myassembly.call("Controller", "go");

    mymodel.dot_to_file();
    mymodel.print();
}

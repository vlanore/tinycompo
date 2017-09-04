#include <algorithm>
#include <iostream>
#include <sstream>
#include <tinycompo.hpp>
#include <vector>

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
    Model<> mymodel;
    mymodel.component<ConstantText>("MyText", "Hello, I'm a rabbit.\nI like carrots.\n");
    mymodel.component<ReplaceChar>("ReplaceAbyB", 'a', 'b');
    mymodel.component<ReplaceChar>("ReplaceBbyD", 'b', 'd');
    mymodel.component<ProcessAndPrint>("Controller");
    mymodel.connect<UseProvide<TextProcessor>>(Address("Controller"), "effect",
                                               Address("ReplaceAbyB"));
    mymodel.connect<UseProvide<TextProcessor>>(Address("Controller"), "effect",
                                               Address("ReplaceBbyD"));
    mymodel.connect<UseProvide<TextSource>>(Address("Controller"), "source", Address("MyText"));

    Assembly<> myassembly(mymodel);
    myassembly.call("Controller", "go");
}

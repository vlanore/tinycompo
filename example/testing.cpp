#include "tinycompo.hpp"

struct MyComposite : public Composite<int> {};

struct MyCompo : public Component {
    MyCompo* buddy{nullptr};
    MyCompo() { port("buddy", &MyCompo::setBuddy); }
    void setBuddy(MyCompo* buddyin) { buddy = buddyin; }
};

int main() {
    Model<> model;
    model.component<MyCompo>("mycompo");
    model.composite<MyComposite>("composite");
    model.component<MyCompo>(Address("composite", 2));
    model.connect<Use<MyCompo>>(Address("mycompo"), "buddy", Address("composite", 2));

    std::ofstream file;
    file.open("tmp.dot");
    model.dot(file);
    file.close();
}

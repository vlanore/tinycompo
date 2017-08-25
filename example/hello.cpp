#include <cstdio>         // for the printf function
#include "tinycompo.hpp"  // this is the only include required for tinycompo applications

// Declaration of a tinycompon component class called Hello
class Hello : public Component {  // must inherit from the Component class
  public:
    // ports must be declared in the constructor using the set method (from Component)
    // each port has a name (here "hello_port") and a pointer to a method (here the sayHello method)
    Hello() { port("hello_port", &Hello::sayHello); }

    // this method is required ; it returns a debug string used to identify the type (and possibly
    // the state) of an instance of the component
    std::string _debug() const { return "Hello_type"; }

    // this is a custom component method; this method is the one that is associated to the port
    // "hello_port" in the constructor
    void sayHello() { printf("Hello world!\n"); }
};

int main() {
    // in order to use the Hello component class, we must first declare an "Model", i.e., a
    // description of the desired component instances and their configuration
    Model<> mymodel;  // here, we declare an empty model called mymodel
    // then, we declare a single component which is an instance of class Hello called "component0"
    mymodel.component<Hello>("component0");

    // The model declared above is just a declaration and did not actually instantiate any component
    // in order to get an instantiated component assembly, we must use the Assembly class and give
    // it a model as parameter:
    Assembly<> myassembly(mymodel);  // here, we declare an assembly based on model mymodel
    // the constructor of the asembly class takes care of instantiating everything declared in the
    // model; in our case, it will instantiate one component from class Hello

    // Now that our component has been instantiated, we may wish to call its sayHello method
    // in order to do that, we can call the "hello_port" port which is associated to said method
    // this is performed using the call method of the Assembly object:
    myassembly.call("component0", "hello_port");  // prints "Hello world!" in console
}

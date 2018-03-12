# tinycompo

[![Build Status](https://travis-ci.org/vlanore/tinycompo.svg?branch=master)](https://travis-ci.org/vlanore/tinycompo) [![Coverage Status](https://coveralls.io/repos/github/vlanore/tinycompo/badge.svg?branch=master)](https://coveralls.io/github/vlanore/tinycompo?branch=master) [![licence CeCILL](https://img.shields.io/badge/license-CeCILL--B-blue.svg)](http://www.cecill.info/licences.en.html)

__tinycompo__ is a lightweight framework to write component-based applications in C++.

## How to use
__tinycompo__ is a header-only framework. To use it, simply add `tinycompo.hpp` to your project and include it where necessary. The license that __tinycompo__ uses (CeCILL-B) should be compatible with direct inclusion in most,, if not all, projects.

Note that __tinycompo__ requires C++11.
Be sure to use a C++11-compatible compiler (e.g., gcc 5) and to use the `--std=c++11` compilation flag (or the flag for a more recent version like C++14).

## A brief presentation
__tinycompo__ proposes to write object-oriented programs, or parts of object-oriented programs, as _component assemblies_.
A component assembly is a set of object instances connected together, typically by holding pointers to one another.
An object in this context, because it is meant to be _composed_ with other objects, is called a _component_.

Instead of instantiating objects directly, __tinycompo__ works by declaring a _model_ of what objects to instantiate and how to connect them together. Such a model can then be instantiated into a concrete _assembly_.

### Advantages and drawbacks
Advantages of the __tinycompo__ approach include:
* Encourages _composition over inheritance_.
* Provides a high-level view of application structure.
* By decoupling assembly declaration and instantiation, 

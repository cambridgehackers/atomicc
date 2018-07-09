
#AtomicC Language Reference

## Introduction

AtomicC is a structural hardware description language that extends C++
with Bluespec-style modules, rules, interfaces, and methods.

AtomicC is structural in that all state elements in the hardware
netlist are explicit in the source code of the design. AtomicC is a
timed HDL, using SystemC terminology. Atomic actions (rules and method
invocations) execute in a single clock cycle.

Like Connectal, AtomicC designs may include both hardware and
software, using interfaces to specify hardware/software communication
in a type safe way. The AtomicC compiler generates the code to pass
arguments between hardware and software.

AtomicC execution consists of 3 phases: netlist generation, netlist
compilation or implementation, and runtime.  During netlist
generation, modules are instantiated by executing their
constructors. During this phase, any C++ constructs may be used, but
the resulting netlist may only contain synthesizeable components.

During netlist compilation, the netlist is analyzed and translated to
an intermediate representation and then to Verilog for simulation or
synthesis. Alternate translations are possible: to native code via
LLVM, to System C, to Gallina for formal verification with the Coq
Proof Assistant, etc.


## Notes on compilation

The design is separated into modules that can export and import interfaces to other modules.
Each source language module compiles into a single verilog module.  Modules are independantly
compiled, depending only on the interface definitions for referenced modules.
Referencing modules do not depend on the internal implementation of referenced modules,
even if they textually exist in the same compilation unit.

To permit reasonable analysis of the program behavior, rules (transactions) can only be
executed in a "sequentially consistent" manner.  Since concurrent rules are all executed in a
single clock cycle, in practice this means that we have to prove at compilation time that
all possible executions can always be considered as some linear sequentially ordered instantiation
within a cycle.

## Additions to C++ syntax

AtomicC incorporates several concepts from BSV: interfaces, modules, guards on methods, and rules.

### __interface

An AtomicC interface is essentially an abstract class similar to a
Java interface. All the methods are virtual and no default
implementations are provided. AtomicC style uses composition of
interfaces rather than inheritance.

The __interface keyword defines a list of methods that are exposed from an object.
Instead of using object inheritance to define reusable interfaces,
they are defined/exported explicitly by objects, allowing fine-grained
specification of interface method visibility.

Methods of a module are translated to value ports for passing the
method arguments and a pair of handshaking ports used for scheduling
method invocations.

References to an object can only be done through interface methods.  State element
declarations inside an object (member variables) are private.

Example:
>        __interface EchoRequest {
>            void say(__int(32) v);
>            void say2(__int(16) a, __int(16) b);
>        };

### __module, __emodule
A module is defined using the keyword "__module", resulting in generation of verilog.
It includes local state elements, interfaces exported, interfaces imported
and rules for clustering operations into atomic units.

Example:
>        __module Echo {
>            EchoRequest      request;               // exported interface
>            EchoIndication   *indication;           // imported interface
>            bool busy;
>            __int(32) itemSay;
>            ...
>            // implementation of method request.say(). Note the guard "if (!busy)".
>            void request.say(__int(32) v) if(!busy) {
>                itemSay = v;
>                ...
>            }
>            void request.saw(__int(16) a, __int(16) b) if(!busy) {
>                ...
>            }
>        };

To reference a separately compiled module, use "__emodule".  These external
module definitions only need to include the exported/imported interfaces.

Example:
>        __module EchoResponder {
>            EchoIndication   indication;           // exported interface
>        };

### guard clauses on methods

Rules are only ready to fire if the rule's guard is true and all the
guards on methods invoked within the rule are also true.

>            void request.say(__int(32) v) if(!busy) {
>                itemSay = v;
>                ...
>            }

### __connect
The __connect statement allows exported interface declarations to be connected
with imported interface references between objects within a module declaration.

Example:
>        __interface ExampleRequest {
>            void say(__int(32) v);
>        };
>
>        __module A {
>             ExampleRequest callIn;
>        };
>
>        __module B {
>            ExampleRequest *callOut;
>        };
>
>        __module C {
>            A consumer;
>            B producer;
>            __connect producer.callOut = consumer.callIn;
>        };

* Comparision with BSV
    The declaration for 'A' is just like BSV.
    In BSV, the declaration for B requires the interface be passed in as
    an interface parameter (forcing a textual ordering to the source code
    declaration sequence).
    <br>
    In AtomicC, the interfaces are stitched together outside in any
    convenient sequence in a location where both the concrete instances
    for A and B are visible.

### To export interfaces from contained objects:

Example:
>        __module CWrapper {
>            A consumer;
>            ExampleRequest request = A.callIn;
>         };

CWrapper just forwards the interface 'request' down into the instance 'consumer'.

### __rule

Rules specify the behavior with a design. A rule operates
transactionally: when a rule's guard and the guards of all of its
method invocations are satisfied, then it is ready to fire. It will be
fire on a clock cycle when it does not conflict with any higher
priority rule. A rule executes atomically.

>        // default guard is true
>        __rule respond_rule {
>            fifo->out.deq();
>            ind->heard(fifo->out.first());
>        }

### integer bit width: __int(A)

### __bitsize
New builtin function to return size in bits of a type or variable.

### reinterpret_cast
This can now be used to cast any datatype to/from __int(A), allowing operations to be performed on a bit level.

## Execution Semantics

### Execution control
#### Asymmetric (ready/enable signalling)
A method/rule is invoked by asserting the "enable" signal.  This signal can only be
asserted if the "ready" signal was valid, allowing the called module to restrict
permissible execution sequences.

#### Symmetric (ready/valid signalling)
Both caller/callee have "able to be executed" signals.  Execution is deemed to take
place in each cycle where both "ready" (from the callee) and "valid" (from the caller)
are asserted.

### Scheduling
Each rule has a set of state elements that it reads and another set of element that it writes.
Valid sequential orderings require that every state element must be logically read before it is logically
written ("read before write").

* Scheduling is done by building a graph:
1. Nodes are rules/guards within a module.
2. For all state elements, insert a directed links from each node that writes the state element to every node that reads it.

* Cycles can be broken in 2 ways:
1. Rules default to have lower priority than methods within a module.  If the designer wants the rule to take precedence, a "priority" statement can be specified.
2. "priority" statements in source text can be used to break cycles, if necessary.

To permit rule scheduling to be dependent on the "enable" signals of methods and other rules, rules use "ready/valid" scheduling.

## debugging with printf

To aid debugging with a simulator, "printf" statements in __module declarations are
translated to "$display" statements in the generated verilog.
For debugging with synthesized hardware, "printf" statements are translated into
indication packets sent through the NOC back to the software side host program.
The format strings for the printf statements are placed into a generated file
in generated/xxx.generated.printf along with a list of the bit lengths for each
parameter to the printf.

To use the NOC printf:
   1) add the following line to the __module being tested:
           __printf;
   2) add a line similar to the following (with the 'xxx' replaced) to the test program:
           atomiccPrintfInit("generated/rulec.generated.printf");

## Interfacing with verilog modules

To reference a module in verilog, fields can be declared in __interface items.
For example:

>        __interface CNCONNECTNET2 {
>            __input  __int(1)         IN1;
>            __input  __int(1)         IN2;
>            __output __int(1)         OUT1;
>            __output __int(1)         OUT2;
>        };
>        __emodule CONNECTNET2 {
>            CNCONNECTNET2 _;
>        };

This will allow references/instantiation of an externally defined verilog module CONNECTNET2
that has 2 'input' ports, IN1 and IN2, as well as 2 'output' ports, OUT1 and OUT2.

### Parameterized modules

Verilog modules that have module instantiation parameters can also be declared/referenced.
For example:

>        __interface Mmcme2MMCME2_ADV {
>            __parameter const char *  BANDWIDTH;
>            __parameter float         CLKFBOUT_MULT_F;
>            __input  __uint(1)        CLKFBIN;
>            __output __uint(1)        CLKFBOUT;
>            __output __uint(1)        CLKFBOUTB;
>        };
>        __emodule MMCME2_ADV {
>            Mmcme2MMCME2_ADV _;
>        };

This example can be instantiated as:

>        __module Test {
>            ...
>            MMCME2_ADV#(BANDWIDTH="WIDE",CLKFBOUT_MULT_F=1.0) mmcm;
>            ...
>            Test() {
>               __rule initRule {
>                   mmcm._.CLKFBIN = mmcm._.CLKFBOUT;
>               }
>            }
>        }

### Reference syntax

For declaring ports in an interface:
>        __interface <interfaceName> {
>             __input/__output/__inout/__parameter <elementType> <elementName>;
>        }
For '__parameter' items, supported datatypes include: "const char *", "float", "int".

### Factoring of interfaces into sub interfaces is also supported.

### Clock/reset ports
Note that if interface port pins are declared in a module interface declaration, then
CLK and nRST are _not_ automatically declared/instantiated.  (Since the user needs the
flexibility to not require them when interfacing with legacy code).

Note that this also allows arbitrary signals (like the output of clock generators) to be
passed to modules as CLK/nRST signals.  (For Atomicc generated modules, please note that the
default clock/reset signals for a module will always have these names)

### Import tooling

There is a tool to automate the creation of AtomicC header files from verilog source files.
For example:
>        atomiccImport -o MMCME2_ADV.h -C MMCME2_ADV -P Mmcme2 zynq.lib
>        atomiccImport -o VMMCME2_ADV.h -C MMCME2_ADV -P Mmcme2 MMCME2_ADV.v

Command line switches...

That's it!

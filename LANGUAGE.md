
#AtomicC Language Reference

## Introduction

The design is separated into modules that can export and import interfaces to other modules.
Each source language module compiles into a single verilog module.  Modules are independantly
compiled, depending only on the interface definitions for referenced modules.
Rreferencing modules do not depend on the internal implementation of referenced modules,
even if they textually exist in the same compilation unit.

To permit reasonable analysis of the program behavior, rules (transctions) can only be
executed in a "sequentially consistent" manner.  Since concurrent rules are all executed in a
single clock cycle, in practice this means that we have to prove at compilation time that
all possible executions can always be considered as some linear sequentially ordered instantiation
within a cycle.

## Additions to C++ syntax :

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

### __interface
    This defines a list of methods that are exposed from an object.  Instead of using object
    inheritance to define reusable interfaces, they are defined/exported explicitly by
    objects, allowing fine-grained specification of interface method visibility.
    Example:
>        __interface EchoRequest {
>            void say(__int(32) v);
>            void say2(__int(16) a, __int(16) b);
>        };

### guard clauses on methods

### __connect

the __connect statement should be between declarations like:

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

The declaration for 'A' is just like BSV.
In BSV, the declaration for B requires the interface be passed in as
an interface parameter (forcing a textual ordering to the source code
declaration sequence).
In AtomicC, the interfaces are stitched together outside in any
convenient sequence in a location where both the concrete instances
for A and B are visible.

****************************************************
To export interfaces from contained objects:

>        __module CWrapper {
>            A consumer;
>            ExampleRequest request = A.callIn;
>         };

CWrapper just forwards the interface 'request' down into the instance 'consumer'.

### __rule

### integer bit width: __int(A)

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



The design is separated into modules that can export and import interfaces to other modules.
Each source language module compiles into a single verilog module.  Modules are independantly
compiled, depending only on the interface definitions for referenced modules; referencing modules
do not depend on the internal implementation of referenced modules, even if they textually
exist in the same compilation unit.

__interface

__module
__emodule

guard clauses on methods

__connect

the __connect statement should be between declarations like:

Interface ExampleRequest {
    void say(void);
};

__module A {
     ExampleRequest callIn;
}

__module B {
    ExampleRequest *callOut;
}

__module C {
    A consumer;
    B producer;
    __connect producer.callOut = consumer.callIn;
}

The declaration for 'A' is just like BSV.
In BSV, the declaration for B requires the interface be passed in as
an interface parameter (forcing a textual ordering to the source code
declaration sequence).
In AtomicC, the interfaces are stitched together outside in any
convenient sequence in a location where both the concrete instances
for A and B are visible.

****************************************************
To export interfaces from contained objects:

__module CWrapper {
    A consumer;
    ExampleRequest request = A.callIn;
 }

CWrapper just forwards the interface 'request' down into the instance 'consumer'.

__rule

integer bit width: __int(A)

ready/valid signalling

scheduling
   Scheduling is done by building a graph:
       1) Nodes are rules/guards.
       2) For all state elements, insert a directed links from each node that writes the state element to every node that reads it.
       3) "priority" statements in source text can be used to break cycles, if necessary.
   For SC to hold, this graph must have no cycles, since we would then be unable to guarantee "read before write".



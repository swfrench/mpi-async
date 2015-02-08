# Asynchronous remote tasks over MPI

This version is really just a proof-of-concept to get local task management
and template magic working correctly (e.g. embedding the correct function
and argument-wrapper types).

As such it's based purely on (speculative) MPI-1 non-blocking two-sided
comms executed by a progress thread (the latter using a number of C++11
features).

The obvious (and most interesting) TODO's are:

1. Better code generation for the type-embedding wrapper structs (either
   brute force generation via a shell script or the like, or maybe there is
   something that can be done with variadic templates).

2. Re-implementation of the comms to use MPI RMA, which will require some
   kind of one-sided message queue (the callback counter can easily be
   implemented directly).

Even after (2) is implemented, the progress thread will likely remain a
persistent feature, as it is also responsible for executing enqueued tasks.


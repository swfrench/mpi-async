/*
 * Asynchronous remote tasks over MPI
 * 
 * This version is really just a proof-of-concept to get local task management
 * and template magic working correctly (e.g. embedding the correct function
 * and argument-wrapper types).
 * 
 * As such it's based purely on (speculative) MPI-1 non-blocking two-sided
 * comms executed by a progress thread (the latter using a number of C++11
 * features).
 * 
 * The obvious (and most interesting) TODO's are:
 * 1. Better code generation for the type-embedding wrapper structs (maybe
 * there is something that can be done with variadic templates).
 * 2. Re-implementation of the comms to use MPI RMA, which will require some
 *    kind of one-sided message queue (the callback counter can easily be
 *    implemented directly).
 * Even after (2) is implemented, the progress thread will likely remain a
 * persistent feature, as it is also responsible for executing enqueued tasks.
 */

#pragma once

#include <stdlib.h>
#include <mpi.h>

// generic function pointer used throughout
typedef void (*fptr)(void *);

/*
 * Prototypes from async.cpp
 */

void enqueue(int target, fptr rp, void *tp, size_t sz);
void async_enable(MPI_Comm);
void async_disable();

/*
 * Task argument and callback structures
 */

#include "gen.async_task_data.hpp"

/*
 * Callback wrappers (overloaded; embeds casting to appropriate types)
 */

#include "gen.async_task_run.hpp"

/*
 * User-facing async invocation calls (overloaded)
 */

#include "gen.async_task_invoke.hpp"

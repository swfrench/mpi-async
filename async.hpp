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
 * 1. Better code generation for the type-embedding wrapper structs (either
 *    brute force generation via a shell script or the like, or maybe there is
 *    something that can be done with variadic templates).
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

template<typename F>
struct task0
{
  F func;
  task0(F func_) : func(func_) {}
};

template<typename F, typename T0>
struct task1
{
  F func;
  T0 x0;
  task1(F func_, T0 x0_) : func(func_), x0(x0_) {}
};

template<typename F, typename T0, typename T1>
struct task2
{
  F func;
  T0 x0;
  T1 x1;
  task2(F func_, T0 x0_, T1 x1_) : func(func_), x0(x0_), x1(x1_) {}
};

/*
 * Callback wrappers (overloaded; embeds casting to appropriate types)
 */

template<typename F>
void runner(void *tp)
{
  task0<F> *t = (task0<F> *)tp;
  t->func();
}

template<typename F, typename T0>
void runner(void *tp)
{
  task1<F, T0> *t = (task1<F, T0> *)tp;
  t->func(t->x0);
}

template<typename F, typename T0, typename T1>
void runner(void *tp)
{
  task2<F, T0, T1> *t = (task2<F, T0, T1> *)tp;
  t->func(t->x0, t->x1);
}

/*
 * User-facing async invocation calls (overloaded)
 */

template<typename F>
void async(int target, F f)
{
  task0<F> *t = new task0<F>(f);
  fptr rp = (fptr) runner<F>;
  enqueue(target, rp, (void *)t, sizeof(*t));
}

template<typename F, typename T0>
void async(int target, F f, T0 x0)
{
  task1<F, T0> *t = new task1<F, T0>(f, x0);
  fptr rp = (fptr) runner<F, T0>;
  enqueue(target, rp, (void *)t, sizeof(*t));
}

template<typename F, typename T0, typename T1>
void async(int target, F f, T0 x0, T1 x1)
{
  task2<F, T0, T1> *t = new task2<F, T0, T1>(f, x0, x1);
  fptr rp = (fptr) runner<F, T0, T1>;
  enqueue(target, rp, (void *)t, sizeof(*t));
}

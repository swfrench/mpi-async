#!/bin/bash

set -e

# default max number of function parameters to support
DEFAULT_NUM_PARAMETERS=6

# header file names
ASYNC_TASK_DATA=gen.async_task_data.hpp
ASYNC_TASK_RUN=gen.async_task_run.hpp
ASYNC_TASK_INVOKE=gen.async_task_invoke.hpp

################################################################################
# args

if [ $# -eq 1 ]; then
        NUM_PARAMETERS=$1
else
        NUM_PARAMETERS=$DEFAULT_NUM_PARAMETERS
fi

################################################################################
# setup

# handles preamble and zero parameter counts

# task argument and fptr data
cat << END > $ASYNC_TASK_DATA
// automatically generated $(date)
#pragma once

/*
 * Task argument and callback structures
 */

template<typename F>
struct task0
{
  F func;
  task0(F func_) : func(func_) {}
};

END

# task runners (casts to appropriate task data struct type)
cat << END > $ASYNC_TASK_RUN
// automatically generated $(date)
#pragma once

/*
 * Run wrappers (overloaded; embeds casting to appropriate types)
 */

template<typename F>
void runner(void *tp)
{
  task0<F> *t = (task0<F> *)tp;
  t->func();
}

END

# task invocation
cat << END > $ASYNC_TASK_INVOKE
// automatically generated $(date)
#pragma once

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

END

################################################################################
# higher parameter counts

# loop over argument counts
for n in `seq 1 $NUM_PARAMETERS`; do
        # name of task data struct
        name="task$n"

        # initialize ...

        # full template parameter list
        template="typename F"

        # template type names
        ttypes="F"

        # function arguments only (members of task data struct)
        fargs=""

        # template-typed arguments: with and without types
        proto="F f"
        proto_vars="f"

        # build them up ...
        for t in `seq 0 $((n-1))`; do
                template="$template, typename T$t"
                ttypes="$ttypes, T$t"
                [ -n "$fargs" ] && fargs="$fargs, "
                fargs="${fargs}t->x$t"
                proto="$proto, T$t x$t"
                proto_vars="$proto_vars, x$t"
        done

        # update runner and invocation header files ...

        cat << END >> $ASYNC_TASK_RUN
template<$template>
void runner(void *tp)
{
  $name<$ttypes> *t = ($name<$ttypes> *)tp;
  t->func($fargs);
}

END
        cat << END >> $ASYNC_TASK_INVOKE
template<$template>
void async(int target, $proto)
{
  $name<$ttypes> *t = new $name<$ttypes>($proto_vars);
  fptr rp = (fptr) runner<$ttypes>;
  enqueue(target, rp, (void *)t, sizeof(*t));
}

END

        # update task data structs ...

        cat << END >> $ASYNC_TASK_DATA
template<$template>
struct $name
{
  F func;
END

        # generate constructor and member vars for task data structs
        targs="F func_"
        init="func(func_)"
        for t in `seq 0 $(($n-1))`; do
                echo "  T$t x$t;" >> $ASYNC_TASK_DATA
                targs="$targs, T$t x${t}_"
                init="$init, x$t(x${t}_)"
        done
        cat << END >> $ASYNC_TASK_DATA
  $name($targs) : $init {}
};

END
done

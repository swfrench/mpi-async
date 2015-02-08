#include <stdio.h>
#include <assert.h>
#include <mpi.h>

#include "async.hpp"

void
func0()
{
  printf("[%s] hello!\n", __func__);
}

void
func1(int x)
{
  printf("[%s] x = %i\n", __func__, x);
}

void
func2(int x, int y)
{
  printf("[%s] x + y = %i\n", __func__, x + y);
}

int
main(int argc, char **argv)
{
  int requested, provided, rank, size;

  requested = MPI_THREAD_MULTIPLE;
  MPI_Init_thread(&argc, &argv, requested, &provided);
  assert(provided >= requested);

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  async_enable(MPI_COMM_WORLD);
  async((rank + 1) % size, func0);
  async((rank + 1) % size, func1, 1);
  async((rank + 1) % size, func2, 1, 2);
  async((rank + 1) % size, [rank] () {
    printf("i'm a lambda shipped from rank %i!\n", rank);
  });
  async_disable();

  MPI_Finalize();

  return 0;
}

/*
 * TODO: Clearly, it would be nice to implement this over one-sided MPI RMA in
 * lieu of non-blocking two-sided calls (based on speculative receives).
 * This will require a one-sided task queue implementation, which is a (separate)
 * work in progress.
 */

#include "async.hpp"

#include <list>
#include <thread>
#include <mutex>
#include <atomic>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <mpi.h>

#define ASYNC_MSG_SIZE 512

#define ASYNC_TAG_ENQUEUE 1024
#define ASYNC_TAG_DECREMENT 2048

#define mpi_assert(X) assert(X == MPI_SUCCESS);

typedef unsigned char byte;

struct task
{
	int rank;  // sender or receiver, depending on which queue
	byte *task_data;
	task(int d, byte *t) : rank(d), task_data(t) {}
};

static std::thread *th_progress;

static std::mutex task_queue_mtx;
static std::mutex outgoing_task_queue_mtx;

static std::list<task *> task_queue;
static std::list<task *> outgoing_task_queue;

static std::atomic<bool> done(false);

static std::atomic<int> cb_count(0);

static int my_rank;
static MPI_Comm my_comm;

/*
 * Unpack a task byte array and execute it
 */
void execute(byte *task_data)
{
	fptr rp = *(fptr *)task_data;
	void *tp = (void *)(task_data + sizeof(fptr));
	rp(tp);
}

/*
 * Route the supplied task to the correct queue for exection on the target,
 * fast-pathing to our own local execution queue if we are in fact the target.
 * This is called by the async() task invocation launch functions.
 */
void enqueue(int target, fptr rp, void *tp, size_t sz)
{
	byte *task_data;

	assert(sz + sizeof(fptr) <= ASYNC_MSG_SIZE);
	task_data = new byte[ASYNC_MSG_SIZE];

	memcpy(task_data, (void *)&rp, sizeof(fptr));
	memcpy(task_data + sizeof(fptr), tp, sz);

	if (target == my_rank) {
		task_queue_mtx.lock();
		task_queue.push_back(new task(target, task_data));
		task_queue_mtx.unlock();
	} else {
		outgoing_task_queue_mtx.lock();
		outgoing_task_queue.push_back(new task(target, task_data));
		outgoing_task_queue_mtx.unlock();
	}

	cb_count++;
}

/*
 * Work performed by the progress thread:
 * 1. servicing remotely invoked task messages for which we are the target; these
 *    are routed to the local execution task_queue
 * 2. sending locally invoked task messages to their targets; these are taken
 *    from the outgoin_task_queue
 * 3. servicing remotely invoked callback decrement operations
 * 4. executing enqeued tasks from the task_queue and sending associated remote
 *    callback decrement messages
 * The task thread is started in async_enable().
 *
 * N.B. All enqueued tasks are dropped on the floor when done becomes false.
 * This means that care must be taken to ensure the cb_count has fallen back to
 * zero *everywhere* before signaling progress thread exit in this manner.
 */
void progress()
{
	MPI_Request t_req, cb_req;
	byte task_data_in[ASYNC_MSG_SIZE];
	mpi_assert(MPI_Irecv(task_data_in, ASYNC_MSG_SIZE, MPI_BYTE,
	                     MPI_ANY_SOURCE, ASYNC_TAG_ENQUEUE,
			     my_comm, &t_req));
	mpi_assert(MPI_Irecv(NULL, 0, MPI_BYTE,
	                     MPI_ANY_SOURCE, ASYNC_TAG_DECREMENT,
			     my_comm, &cb_req));
	while (! done) {
		int have_msg;
		MPI_Status stat;
		task *msg;
		byte *task_data;

		// service incoming tasks (enqueue them for execution)
		mpi_assert(MPI_Test(&t_req, &have_msg, &stat));
		if (have_msg) {
			task_data = new byte[ASYNC_MSG_SIZE];
			memcpy(task_data, task_data_in, ASYNC_MSG_SIZE);
			task_queue_mtx.lock();
			task_queue.push_back(new task(stat.MPI_SOURCE,
			                              task_data));
			task_queue_mtx.unlock();
			mpi_assert(MPI_Irecv(task_data_in,
			                     ASYNC_MSG_SIZE, MPI_BYTE,
					     MPI_ANY_SOURCE, ASYNC_TAG_ENQUEUE,
					     my_comm, &t_req));
		}

		// service incoming callback decrements
		mpi_assert(MPI_Test(&cb_req, &have_msg, &stat));
		if (have_msg) {
			cb_count--;
			mpi_assert(MPI_Irecv(NULL, 0, MPI_BYTE,
					     MPI_ANY_SOURCE, ASYNC_TAG_DECREMENT,
					     my_comm, &cb_req));
		}

		// send outgoing tasks
		msg = NULL;
		outgoing_task_queue_mtx.lock();
		if (! outgoing_task_queue.empty()) {
			msg = outgoing_task_queue.front();
			outgoing_task_queue.pop_front();
		}
		outgoing_task_queue_mtx.unlock();
		if (msg != NULL) {
			mpi_assert(MPI_Send(msg->task_data,
			                    ASYNC_MSG_SIZE, MPI_BYTE,
					    msg->rank, ASYNC_TAG_ENQUEUE,
					    my_comm));
			delete [] msg->task_data;
			delete msg;
		}

		// execute enqueued task, possibly sending callback decrement op
		msg = NULL;
		task_queue_mtx.lock();
		if (! task_queue.empty()) {
			msg = task_queue.front();
			task_queue.pop_front();
		}
		task_queue_mtx.unlock();
		if (msg != NULL) {
			execute(msg->task_data);
			if (msg->rank == my_rank)
				cb_count--;
			else
				mpi_assert(MPI_Send(NULL, 0, MPI_BYTE,
				                    msg->rank, ASYNC_TAG_DECREMENT,
				                    my_comm));
			delete [] msg->task_data;
			delete msg;
		}
	}
	// exiting: cancel the outstanding receives
	mpi_assert(MPI_Cancel(&t_req));
	mpi_assert(MPI_Cancel(&cb_req));
}

/*
 * Collective: enable asynchronous task execution among ranks on the supplied
 * communicator
 */
void async_enable(MPI_Comm comm)
{
	int mpi_init, mpi_thread;
	mpi_assert(MPI_Initialized(&mpi_init));
	assert(mpi_init);
	mpi_assert(MPI_Query_thread(&mpi_thread));
	assert(mpi_thread == MPI_THREAD_MULTIPLE);

	my_comm = comm;
	mpi_assert(MPI_Comm_rank(my_comm, &my_rank));

	th_progress = new std::thread(progress);

	mpi_assert(MPI_Barrier(my_comm));
}

/*
 * Collective: Stop invoking asynchronous tasks and block until all outstanding
 * tasks have been executed.
 */
void async_disable()
{
	while (cb_count) {};
	mpi_assert(MPI_Barrier(my_comm));
	done = true;
	th_progress->join();
	delete th_progress;
}

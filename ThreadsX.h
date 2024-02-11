#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>


struct FThreadTask;

#define MAX_THREADS 4

struct FThreadHandle;

struct FThreadMaster
{
	bool bRequestExit;
	FThreadHandle* Handles[MAX_THREADS];

	FThreadMaster()
		: bRequestExit(false)
	{
		for (int i = 0; i < MAX_THREADS; ++i)
			Handles[i] = NULL;
	}
	void QueryTask(FThreadTask* T);
};
struct FThreadHandle
{
	FThreadMaster& Master;
	FThreadTask* TaskList;
	thread Thread;
	mutex TaskMutex;
	int LineSize;
	volatile bool bWaiting;

	void Main();
	void UnlistTask(FThreadTask* Task);
	static void LaunchThread(FThreadHandle* H)
	{
		H->Main();
	}
	void Query(FThreadTask* NewTask);
	FThreadHandle(FThreadMaster& M)
		: Master(M), TaskList(NULL), Thread(FThreadHandle::LaunchThread, this), bWaiting(false)
	{}
	~FThreadHandle()
	{
		Thread.join();
	}
};
struct FThreadTask
{
	FThreadMaster& Master;
	FThreadHandle* Handle;
	FThreadTask* Next;
	volatile bool bRunning : 1, bFinished : 1;

	FThreadTask(FThreadMaster& M)
		: Master(M), Handle(NULL), Next(NULL), bRunning(false), bFinished(false)
	{
		M.QueryTask(this);
	}

	virtual void Main() = 0;

	virtual ~FThreadTask()
	{
		if (!bFinished)
		{
			Handle->TaskMutex.lock();
			if (bRunning)
			{
				Handle->TaskMutex.unlock();
				WaitForFinish();
			}
			else
			{
				Handle->UnlistTask(this);
				Handle->TaskMutex.unlock();
			}
		}
	}
	void WaitForFinish()
	{
		while (!bFinished)
			this_thread::yield();
	}
};
void FThreadMaster::QueryTask(FThreadTask* T)
{
	int i, iLowest = 0;
	for (i = 0; i < MAX_THREADS; ++i)
	{
		if (!Handles[i])
		{
			Handles[i] = new FThreadHandle(*this);
			Handles[i]->Query(T);
			return;
		}
		if (Handles[i]->bWaiting)
		{
			Handles[i]->Query(T);
			return;
		}
		if (i > 0 && Handles[i]->LineSize < Handles[iLowest]->LineSize)
			iLowest = i;
	}
	Handles[iLowest]->Query(T);
}
void FThreadHandle::Main()
{
	FThreadTask* Cur;
	while (!Master.bRequestExit)
	{
		while (bWaiting)
			this_thread::yield();

		while (1)
		{
			TaskMutex.lock();
			Cur = TaskList;
			if (!Cur)
			{
				bWaiting = true;
				TaskMutex.unlock();
				break;
			}
			TaskList = Cur->Next;
			Cur->Next = nullptr;
			Cur->bRunning = true;
			TaskMutex.unlock();
			Cur->Main();
			--LineSize;
			Cur->bFinished = true;
		}
	}
}
inline void FThreadHandle::UnlistTask(FThreadTask* Task)
{
	--LineSize;
	if (TaskList == Task)
		TaskList = Task->Next;
	else
	{
		for (FThreadTask* T = TaskList; T = T->Next; T)
		{
			if (T->Next == Task)
			{
				T->Next = Task->Next;
				break;
			}
		}
	}
}
void FThreadHandle::Query(FThreadTask* NewTask)
{
	++LineSize;
	TaskMutex.lock();
	NewTask->Handle = this;
	if (!TaskList)
		TaskList = NewTask;
	else
	{
		for (FThreadTask* T = TaskList; T = T->Next; T)
		{
			if (!T->Next)
			{
				T->Next = NewTask;
				break;
			}
		}
	}
	bWaiting = false;
	TaskMutex.unlock();
}

// File:  threadPool.h
// Date:  1/10/2018
// Auth:  K. Loux
// Desc:  Thread pool class.

#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

// Standard C++ headers
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <chrono>
#include <memory>
#include <cassert>
#include <type_traits>

template<typename Caller = void>
class ThreadPool
{
public:
	ThreadPool(const unsigned int& threadCount, const unsigned int& rateLimit);
	virtual ~ThreadPool();

	struct JobInfo;
	typedef void(*JobFunction)(const JobInfo&);

	struct JobInfo
	{
		JobInfo() = default;
		explicit JobInfo(JobFunction jobFunction) : jobFunction(jobFunction) {}
		virtual ~JobInfo() = default;

		JobFunction jobFunction;
	};

	void AddJob(std::unique_ptr<JobInfo> job, Caller* caller = nullptr);
	void WaitForAllJobsComplete() const;

private:
	mutable std::mutex queueMutex;
	std::condition_variable jobReadyCondition;
	mutable std::condition_variable jobCompleteCondition;

	std::vector<std::thread> threads;
	const std::chrono::milliseconds minRequestDelta;

	void ThreadEntry();

	// These members are protected by queueMutex
	std::queue<std::pair<std::unique_ptr<JobInfo>, Caller*>> jobQueue;
	std::chrono::steady_clock::time_point lastRequestTime = std::chrono::steady_clock::now();
	unsigned int pendingJobCount = 0;
};

template<typename Caller>
ThreadPool<Caller>::ThreadPool(const unsigned int& threadCount, const unsigned int& rateLimit)
	: minRequestDelta(rateLimit > 0 ? std::chrono::milliseconds(static_cast<long long>(1000.0 / rateLimit)) : std::chrono::milliseconds(0))
{
	threads.resize(threadCount);
	for (auto& t : threads)
		t = std::thread(&ThreadPool::ThreadEntry, this);
}

template<typename Caller>
ThreadPool<Caller>::~ThreadPool()
{
	{
		std::lock_guard<std::mutex> lock(queueMutex);
		while (!jobQueue.empty())
			jobQueue.pop();
	}

	unsigned int i;
	for (i = 0; i < threads.size(); ++i)
		AddJob(nullptr);

	for (auto& t : threads)
	{
		if (t.joinable())
			t.join();
	}
}

template<typename Caller>
void ThreadPool<Caller>::AddJob(std::unique_ptr<JobInfo> job, Caller* caller)
{
	assert((caller && !std::is_same<Caller, void>::value) || (!caller && std::is_same<Caller, void>::value));

	{
		std::lock_guard<std::mutex> lock(queueMutex);
		jobQueue.push(std::make_pair(std::move(job), caller));
	}

	jobReadyCondition.notify_one();
}

template<typename Caller>
void ThreadPool<Caller>::WaitForAllJobsComplete() const
{
	std::unique_lock<std::mutex> lock(queueMutex);
	jobCompleteCondition.wait(lock, [this]()
	{
		return jobQueue.empty() && pendingJobCount == 0;
	});
}

template<typename Caller>
void ThreadPool<Caller>::ThreadEntry()
{
	while (true)
	{
		std::unique_ptr<JobInfo> thisJob;
		std::chrono::milliseconds sleepTime(0);
		Caller* caller;

		{
			std::unique_lock<std::mutex> lock(queueMutex);
			jobReadyCondition.wait(lock, [this]()
			{
				return jobQueue.size() > 0;
			});

			caller = jobQueue.front().second;
			thisJob = std::move(jobQueue.front().first);
			++pendingJobCount;
			jobQueue.pop();

			if (minRequestDelta > std::chrono::milliseconds(0))
			{
				std::chrono::steady_clock::time_point now(std::chrono::steady_clock::now());
				if (now - lastRequestTime < minRequestDelta)
					sleepTime = minRequestDelta - std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRequestTime);
				else
					sleepTime = std::chrono::milliseconds(0);
				lastRequestTime = now;
			}
		}

		if (!thisJob)
			break;

		std::this_thread::sleep_for(sleepTime);
		if (caller)
			(caller->*(caller->jobFunction))(*thisJob);
		else
			thisJob->jobFunction(*thisJob);

		{
			std::lock_guard<std::mutex> lock(queueMutex);
			--pendingJobCount;
		}

		jobCompleteCondition.notify_one();
	}

	std::lock_guard<std::mutex> lock(queueMutex);
	--pendingJobCount;
}


#endif// THREAD_POOL_H_

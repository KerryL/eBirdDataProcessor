// File:  throttledSection.cpp
// Date:  1/29/2018
// Auth:  K. Loux
// Desc:  Tool for managing access to a resource with a rate limit.

// Local headers
#include "throttledSection.h"

// Standard C++ headers
#include <thread>

ThrottledSection::ThrottledSection(const Clock::duration& minAccessDelta) : minAccessDelta(minAccessDelta)
{
}

void ThrottledSection::Wait()
{
	std::lock_guard<std::mutex> lock(mutex);
	const Clock::time_point now(Clock::now());
	if (now - lastAccess < minAccessDelta)
		std::this_thread::sleep_for(minAccessDelta - (now - lastAccess));

	lastAccess = Clock::now();
}

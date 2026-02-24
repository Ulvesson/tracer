// tracer.cpp : Defines the entry point for the application.
//

#include "tracer.h"

#include <cmath>
#include <iostream>
#include <thread>
#include <vector>

// --- Example workloads to demonstrate tracing ---

void simulateWork(const std::string& label, int ms)
{
	TRACE_SCOPE(label);
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void loadAssets()
{
	TRACE_FUNCTION();
	simulateWork("LoadTextures", 30);
	simulateWork("LoadMeshes", 50);
	simulateWork("LoadShaders", 20);
}

void computePhysics()
{
	TRACE_FUNCTION();

	TRACE_BEGIN("BroadPhase", "physics");
	std::this_thread::sleep_for(std::chrono::milliseconds(15));
	TRACE_END("BroadPhase", "physics");

	TRACE_BEGIN("NarrowPhase", "physics");
	std::this_thread::sleep_for(std::chrono::milliseconds(25));
	TRACE_END("NarrowPhase", "physics");
}

double heavyMath(int iterations)
{
	TRACE_FUNCTION();
	double result = 0.0;
	for (int i = 1; i <= iterations; ++i)
		result += std::sin(static_cast<double>(i)) * std::cos(static_cast<double>(i));
	return result;
}

void workerThread(int id)
{
	std::string name = "Worker_" + std::to_string(id);
	TRACE_SCOPE(name);

	simulateWork(name + "_TaskA", 20 + id * 10);
	heavyMath(500'000);
	simulateWork(name + "_TaskB", 10 + id * 5);
}

int main()
{
	// Start the tracing session — all events go to this file
	ChromeTracer::instance().beginSession("trace.json");

	{
		TRACE_SCOPE("Main");

		TRACE_INSTANT("AppStart", "lifecycle");

		// Single-threaded work
		loadAssets();
		computePhysics();

		// Multi-threaded work — each thread appears as its own row in the timeline
		{
			TRACE_SCOPE("ParallelSection");
			std::vector<std::thread> threads;
			for (int i = 0; i < 4; ++i)
				threads.emplace_back(workerThread, i);
			for (auto& t : threads)
				t.join();
		}

		TRACE_INSTANT("AppEnd", "lifecycle");
	}

	// Flush all events to trace.json
	ChromeTracer::instance().endSession();

	std::cout << "Trace written to trace.json\n"
		<< "Open chrome://tracing or https://ui.perfetto.dev and load the file.\n";

	return 0;
}

// tracer.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// ============================================================================
// Chrome Trace Event Format logger
// Output: JSON loadable in chrome://tracing or https://ui.perfetto.dev
// ============================================================================

struct TraceEvent
{
	std::string name;
	std::string category;
	char phase;                          // 'B' = begin, 'E' = end, 'X' = complete, 'I' = instant
	double timestampUs;                  // microseconds since session start
	double durationUs;                   // only used for 'X' (complete) events
	std::thread::id threadId;
};

class ChromeTracer
{
public:
	// Type aliases — must be at the top for use in public methods
	using clock = std::chrono::high_resolution_clock;
	using TimePoint = clock::time_point;

	static ChromeTracer& instance();

	// Call once at program start
	void beginSession(const std::string& filepath = "trace.json");

	// Call once at program end — writes the JSON file
	void endSession();

	// Record a complete duration event (phase 'X')
	void addDurationEvent(const std::string& name,
		const std::string& category,
		TimePoint start,
		TimePoint end);

	// Record a begin ('B') or end ('E') event for manual pairing
	void addBeginEvent(const std::string& name, const std::string& category);
	void addEndEvent(const std::string& name, const std::string& category);

	// Record an instant event ('I')
	void addInstantEvent(const std::string& name, const std::string& category);

	// Static accessor for current time
	static TimePoint now() { return clock::now(); }

private:
	using time_point = clock::time_point;

	ChromeTracer() = default;
	~ChromeTracer();
	ChromeTracer(const ChromeTracer&) = delete;
	ChromeTracer& operator=(const ChromeTracer&) = delete;

	void addPhaseEvent(const std::string& name,
		const std::string& category,
		char phase);

	void writeToFile() const;

	std::mutex m_mutex;
	std::vector<TraceEvent> m_events;
	time_point m_startTime;
	std::string m_filepath;
	bool m_active = false;
};

// ============================================================================
// RAII scope timer — records a complete event on destruction
// ============================================================================

class ScopeTrace
{
public:
	ScopeTrace(const std::string& name, const std::string& category = "function");
	~ScopeTrace();

	ScopeTrace(const ScopeTrace&) = delete;
	ScopeTrace& operator=(const ScopeTrace&) = delete;

private:
	std::string m_name;
	std::string m_category;
	ChromeTracer::TimePoint m_start;
};

// ============================================================================
// Convenience macros — compile out when TRACER_DISABLED is defined
// ============================================================================

#ifndef TRACER_DISABLED

#define TRACE_FUNCTION()       ScopeTrace _trace_##__LINE__(__FUNCTION__)
#define TRACE_SCOPE(name)      ScopeTrace _trace_##__LINE__((name))
#define TRACE_BEGIN(name, cat)  ChromeTracer::instance().addBeginEvent((name), (cat))
#define TRACE_END(name, cat)   ChromeTracer::instance().addEndEvent((name), (cat))
#define TRACE_INSTANT(name, cat) ChromeTracer::instance().addInstantEvent((name), (cat))

#else

#define TRACE_FUNCTION()       ((void)0)
#define TRACE_SCOPE(name)      ((void)0)
#define TRACE_BEGIN(name, cat)  ((void)0)
#define TRACE_END(name, cat)   ((void)0)
#define TRACE_INSTANT(name, cat) ((void)0)

#endif

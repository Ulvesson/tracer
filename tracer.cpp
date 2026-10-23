#include "tracer.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

ChromeTracer& ChromeTracer::instance()
{
	static ChromeTracer s_instance;
	return s_instance;
}

ChromeTracer::~ChromeTracer()
{
	endSession();
}

void ChromeTracer::beginSession(const std::string& filepath)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_filepath = filepath;
	m_events.clear();
	m_startTime = clock::now();
	m_active = true;
}

void ChromeTracer::endSession()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_active)
		return;
	m_active = false;
	writeToFile();
}

void ChromeTracer::addDurationEvent(const std::string& name,
	const std::string& category,
	TimePoint start,
	TimePoint end)
{
	using us = std::chrono::duration<double, std::micro>;
	TraceEvent ev;
	ev.name = name;
	ev.category = category;
	ev.phase = 'X';
	ev.timestampUs = std::chrono::duration_cast<us>(start - m_startTime).count();
	ev.durationUs = std::chrono::duration_cast<us>(end - start).count();
	ev.threadId = std::this_thread::get_id();

	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_active)
		m_events.push_back(std::move(ev));
}

void ChromeTracer::addBeginEvent(const std::string& name, const std::string& category)
{
	addPhaseEvent(name, category, 'B');
}

void ChromeTracer::addEndEvent(const std::string& name, const std::string& category)
{
	addPhaseEvent(name, category, 'E');
}

void ChromeTracer::addInstantEvent(const std::string& name, const std::string& category, char scope)
{
	TraceEvent ev;
	ev.name = name;
	ev.category = category;
	ev.phase = 'i';
	ev.scope = (scope == 'p' || scope == 'g') ? scope : 't';

	using us = std::chrono::duration<double, std::micro>;
	ev.timestampUs = std::chrono::duration_cast<us>(clock::now() - m_startTime).count();
	ev.durationUs = 0.0;
	ev.threadId = std::this_thread::get_id();

	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_active)
		m_events.push_back(std::move(ev));
}

void ChromeTracer::addArg(const std::string& name, const std::string& value)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_events.empty())
	{
		TraceEvent& ev = m_events.back();
		ev.args.push_back(std::make_pair(name, value));
	}
}

void ChromeTracer::addPhaseEvent(const std::string& name,
	const std::string& category,
	char phase)
{
	using us = std::chrono::duration<double, std::micro>;
	TraceEvent ev;
	ev.name = name;
	ev.category = category;
	ev.phase = phase;
	ev.timestampUs = std::chrono::duration_cast<us>(clock::now() - m_startTime).count();
	ev.durationUs = 0.0;
	ev.threadId = std::this_thread::get_id();

	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_active)
		m_events.push_back(std::move(ev));
}

void ChromeTracer::writeToFile() const
{
	const auto escapeJsonString = [](const std::string& text) {
		std::string escaped;
		escaped.reserve(text.size());
		for (char ch : text)
		{
			switch (ch)
			{
			case '\\': escaped += "\\\\"; break;
			case '"': escaped += "\\\""; break;
			case '\n': escaped += "\\n"; break;
			case '\r': escaped += "\\r"; break;
			case '\t': escaped += "\\t"; break;
			default: escaped += ch; break;
			}
		}
		return escaped;
	};

	const auto serializeArgValue = [&escapeJsonString](const std::string& value) {
		char* endPtr = nullptr;
		const double parsed = std::strtod(value.c_str(), &endPtr);
		const bool isNumber = endPtr != value.c_str()
			&& *endPtr == '\0'
			&& std::isfinite(parsed);

		if (isNumber)
		{
			std::ostringstream numStream;
			numStream << std::defaultfloat << parsed;
			return numStream.str();
		}

		return std::string("\"") + escapeJsonString(value) + "\"";
	};

	std::ofstream ofs(m_filepath);
	ofs << std::fixed << std::setprecision(3);
	ofs << "{\"traceEvents\":[";

	for (size_t i = 0; i < m_events.size(); ++i)
	{
		const auto& ev = m_events[i];
		if (i > 0)
			ofs << ",";

		// Thread id → numeric
		std::ostringstream tidStream;
		tidStream << ev.threadId;

		ofs << "{\"name\":\"" << escapeJsonString(ev.name)
			<< "\",\"cat\":\"" << escapeJsonString(ev.category)
			<< "\",\"ph\":\"" << ev.phase
			<< "\",\"ts\":" << ev.timestampUs
			<< ",\"pid\":0"
			<< ",\"tid\":" << tidStream.str();

		if (ev.phase == 'i')
			ofs << ",\"s\":\"" << ev.scope << "\"";

		if (ev.phase == 'X')
			ofs << ",\"dur\":" << ev.durationUs;

		if (!ev.args.empty())
		{
			ofs << ",\"args\":{";
			for (size_t argIndex = 0; argIndex < ev.args.size(); ++argIndex)
			{
				if (argIndex > 0)
					ofs << ",";

				const auto& arg = ev.args[argIndex];
				ofs << "\"" << escapeJsonString(arg.first) << "\":"
					<< serializeArgValue(arg.second);
			}
			ofs << "}";
		}

		ofs << "}";
	}

	ofs << "]}";
}

// ============================================================================
// ScopeTrace implementation
// ============================================================================

ScopeTrace::ScopeTrace(const std::string& name, const std::string& category)
	: m_name(name), m_category(category), m_start(ChromeTracer::now())
{
}

ScopeTrace::~ScopeTrace()
{
	ChromeTracer::instance().addDurationEvent(
		m_name, m_category, m_start, ChromeTracer::now());
}
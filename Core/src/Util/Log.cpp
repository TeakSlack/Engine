#include "Log.h"
#include <spdlog/sinks/stdout_color_sinks.h>

Log& Log::Get()
{
	static Log s_Instance;
	return s_Instance;
}

void Log::Init()
{
	auto color_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	color_sink->set_pattern("%^[%H:%M:%S.%e] [%n]%$ %v");

	m_Sinks.push_back(color_sink);

	// Create the default "core" logger at startup.
	auto core_logger = std::make_shared<spdlog::logger>("core", color_sink);
	core_logger->set_level(spdlog::level::trace);
	core_logger->flush_on(spdlog::level::warn);
	spdlog::register_logger(core_logger);
	m_Loggers.push_back(core_logger);
}

spdlog::logger* Log::GetLogger(const std::string& name)
{
	// Find an existing logger with the given name.
	for (const auto& logger : m_Loggers)
	{
		if (logger->name() == name)
			return logger.get();
	}

	// If not found, create a new logger with the same sinks as the core logger.
	auto core_logger = m_Loggers.front();
	auto new_logger = std::make_shared<spdlog::logger>(name, m_Sinks.begin(), m_Sinks.end());
	new_logger->set_level(core_logger->level());
	new_logger->flush_on(core_logger->flush_level());
	spdlog::register_logger(new_logger);
	m_Loggers.push_back(new_logger);
	return new_logger.get();
}

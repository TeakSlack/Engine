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
	m_Loggers.insert({"core", core_logger});
}

spdlog::logger* Log::GetLogger(const std::string& name)
{
	auto it = m_Loggers.find(name);
	if (it != m_Loggers.end())
		return it->second.get();

	// Not found — create a new logger inheriting the core logger's settings.
	const auto& core = m_Loggers.at("core");
	auto new_logger = std::make_shared<spdlog::logger>(name, m_Sinks.begin(), m_Sinks.end());
	new_logger->set_level(core->level());
	new_logger->flush_on(core->flush_level());
	spdlog::register_logger(new_logger);
	m_Loggers.emplace(name, new_logger);
	return new_logger.get();
}

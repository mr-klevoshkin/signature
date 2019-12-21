#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>

namespace logging
{
#define write_log(typ, mdl, msg) logger::print_msg(typ, mdl, msg);

#define LG_INFO		logger::INF
#define LG_SUCCESS	logger::SUC
#define LG_DEBUG	logger::DBG
#define LG_WARNING	logger::WRN
#define LG_ERROR	logger::ERR

#define SHOW_DEBUG

	static class logger
	{
	private:
		static std::ostream* sp_out;
		static std::ostream* sp_err;

	public:
		static enum msg_type
		{
			INF,
			SUC,
			DBG,
			WRN,
			ERR
		};

		static void configure(std::ostream*);
		static void configure(std::ostream*, std::ostream*);

		static void print_msg(msg_type, const std::string, const std::string);
	};
}

#endif // LOGGER_H


#include "logger.h"

#include <sstream>
#include <iomanip>

#pragma warning(disable : 4996)

#define CASE_TYPE_RETURN_STRING(type) case logger::msg_type::type: return #type;

using namespace std;

namespace logging
{
	// -----------------

	ostream* logger::sp_out = &cout;
	ostream* logger::sp_err = &cerr;

	// -----------------

	static inline const tm* get_time()
	{
		auto t = time(nullptr);
		return localtime(&t);
	}

	// -----------------

	static inline const string get_type(logger::msg_type type)
	{
		switch (type)
		{
		CASE_TYPE_RETURN_STRING(DBG)
		CASE_TYPE_RETURN_STRING(INF)
		CASE_TYPE_RETURN_STRING(SUC)
		CASE_TYPE_RETURN_STRING(WRN)
		CASE_TYPE_RETURN_STRING(ERR)
		}
	}

	// -----------------

	void logger::configure(ostream* out_stream)
	{
		logger::configure(out_stream, out_stream);
	}
	
	// -----------------

	void logger::configure(ostream* out_stream, ostream* err_stream)
	{
		logger::sp_out = out_stream;
		logger::sp_err = err_stream;
	}

	// -----------------

	void logger::print_msg(msg_type type, const string module, const string message)
	{
		stringstream ss;
		ss << "(" << get_type(type) << ") [" << put_time(get_time(), "%F %X") << "] " << module << " : " << message;

		switch (type)
		{
		case (msg_type::INF):
		case (msg_type::SUC):
#ifdef SHOW_DEBUG
		case (msg_type::DBG):
#endif // SHOW_DEBUG
			*sp_out << ss.str() << endl;
			break;

		case (msg_type::WRN):
		case (msg_type::ERR):
			*sp_err << ss.str() << endl;
			break;
		}
	}
}
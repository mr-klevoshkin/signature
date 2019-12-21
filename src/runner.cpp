#include "runner.h"

#include <csignal>
#include <iostream>

using namespace std;

namespace running
{
	// --------------------

	void(*runner::_p_stop_fn)(int, const string, void*) = {};
	void* runner::_args = nullptr;

	// --------------------

	map<int, runner::SIGNAL_MODE> runner::_signals = {
		make_pair(SIGTERM, SIGNAL_MODE::success), // Termination request
		make_pair(SIGINT, SIGNAL_MODE::success), // CTRL + C signal

		make_pair(SIGSEGV, SIGNAL_MODE::failure), // Illegal storage access
		make_pair(SIGILL, SIGNAL_MODE::failure), // Illegal instruction
		make_pair(SIGABRT, SIGNAL_MODE::failure), // Abnormal termination
		make_pair(SIGFPE, SIGNAL_MODE::failure), // Floating - point error
	};

	// --------------------

#define CASE_SIG_RETURN_MSG(c) case c: return "External signal: " + string( #c );;

	string runner::get_signal_message(int error_code)
	{
		switch (error_code)
		{
				CASE_SIG_RETURN_MSG(SIGTERM)
				CASE_SIG_RETURN_MSG(SIGINT)
				CASE_SIG_RETURN_MSG(SIGSEGV)
				CASE_SIG_RETURN_MSG(SIGILL)
				CASE_SIG_RETURN_MSG(SIGABRT)
				CASE_SIG_RETURN_MSG(SIGFPE)
		}
	}

	// --------------------

	void runner::handler(int error_code)
	{
		const string message = get_signal_message(error_code);
		switch (_signals[error_code])
		{
		case SIGNAL_MODE::success:
			_p_stop_fn(EXIT_SUCCESS, message, _args);
			break;

		case SIGNAL_MODE::ignor:
			break;

		case SIGNAL_MODE::failure:
			_p_stop_fn(EXIT_FAILURE, message, _args);
			exit(EXIT_FAILURE);
			break;
		}
	}

	// --------------------

	bool runner::safe_invoke(function<void()> action, void(*p_stop_fn)(int, const string, void*), void* arg)
	{
		_p_stop_fn = p_stop_fn;
		_args = arg;

		/** Parsing system signals */
		signal(SIGTERM, handler);
		signal(SIGINT, handler);
		signal(SIGSEGV, handler);
		signal(SIGILL, handler);
		signal(SIGABRT, handler);
		signal(SIGFPE, handler);

		try
		{
			action();
			return true;
		}
		catch(exception e)
		{
			_p_stop_fn(EXIT_FAILURE, "Exception thrown: " + string(e.what()), arg);
			return false;
		}
	}

	// --------------------

}
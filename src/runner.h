#ifndef RUNNER_H
#define RUNNER_H

#include <functional>
#include <string>
#include <map>

namespace running
{
	static class runner
	{
	private:
		enum class SIGNAL_MODE
		{
			success,
			ignor,
			failure
		};

		static std::map<int, SIGNAL_MODE> _signals;
		static void(*_stop_fn)(int, const std::string, void*);
		static void* _args;
		static void handler(int);
		static std::string get_signal_message(int);

	public:
		static bool safe_invoke(std::function<void()>, void(*)(int, const std::string, void*), void*);
	};
}

#endif // RUNNER_H
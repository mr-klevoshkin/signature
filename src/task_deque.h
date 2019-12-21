#ifndef TASK_DEQUE_H
#define TASK_DEQUE_H

#include <future>
#include <string>
#include <list>
#include <thread>
#include <vector>
#include <sstream>
#include <functional>

#include "logger.h"

#define CaseStatusReturnString(status) case server_status::status: return #status;

using namespace logging;

namespace taskking
{
	/// ================================================================================================

	template<class ReturnType, class Args>
	class task
	{
	private:
		bool __is_run;
		bool __is_done;
		int __id;

		Args __args;
		std::function<ReturnType(Args)> __function;
		std::future<ReturnType> __future;

	public:

		// --------------------

		task(int id, std::function<ReturnType(Args)> func, Args args) :
			__id(id),
			__function(func),
			__args(args),
			__is_run(false),
			__is_done(false)
		{}

		~task() {};

		// --------------------

		bool try_run_task()
		{
			if (__is_run || __is_done)
			{
				return false;
			}

			__is_run = true;
			__future = async(std::launch::async, __function, __args);
			return true;
		}

		// --------------------

		bool try_get_data(ReturnType& data)
		{
			if (__is_done)
			{
				throw "Data was already used";
			}

			if (!__is_run)
			{
				return false;
			}

			if (__future.wait_for(chrono::seconds(0)) == future_status::ready)
			{
				data = __future.get();
				__is_done = true;

				return true;
			}

			return false;
		}

		// --------------------

		int id()
		{
			return __id;
		}
	};

	/// ================================================================================================

	template<class ReturnType, class Args>
	class task_deque
	{
	private:

		enum class server_status
		{
			starting,
			running,
			stopping,
			stopped,
			terminating,
			undefinded
		};

		std::string server_string(server_status st)
		{
			switch (st)
			{
				CaseStatusReturnString(starting)
				CaseStatusReturnString(running)
				CaseStatusReturnString(stopping)
				CaseStatusReturnString(stopped)
				CaseStatusReturnString(terminating)
				CaseStatusReturnString(undefinded)
			}
		}

		server_status _status;
		std::list<std::shared_ptr<task<ReturnType, Args>>> * _p_buffer;
		
		std::mutex _mutex;
		std::thread _update_thread;

		void* _args;
		void(*_p_callback)(int, ReturnType, void*);
		
		size_t _max_tasks;
		time_t _timeout;

		std::atomic_int _run_tasks_count;
		std::atomic_int _tasks_in_queue_count;

		// --------------------

		void do_update()
		{
			_mutex.lock();
			_p_buffer->remove_if([&](std::shared_ptr<task<ReturnType, Args>> shrp_task) -> bool
				{
					task<ReturnType, Args>* p_task = shrp_task.get();

					ReturnType data;
					if (p_task->try_get_data(data))
					{
						_p_callback(p_task->id(), std::move(data), std::move(_args));
						_run_tasks_count--;
						return true;
					}

					if (can_run_task() && p_task->try_run_task())
					{
						_tasks_in_queue_count--;
						_run_tasks_count++;
					}

					return false;
				});
			_mutex.unlock();
		}

		// --------------------

		void update()
		{
			update_status(server_status::running);
			while (_status == server_status::running)
			{	
				std::this_thread::sleep_for(std::chrono::milliseconds(_timeout));

				if (is_empty())
				{
					stop();
				}
				else
				{
					do_update();
				}

				write_log(LG_DEBUG, "DEQUE", "Tasks in queue: " + to_string(_tasks_in_queue_count) + "; Tasks running: " + to_string(_run_tasks_count));
			}
			update_status(server_status::stopped);
		}

		// --------------------

		void run()
		{
			if (_status != server_status::stopped)
			{
				return;
			}

			update_status(server_status::starting);
			_update_thread = std::thread(&task_deque::update, this);
			// _update_thread.detach();
		}

		// --------------------

		void stop()
		{
			if (_status != server_status::starting && _status != server_status::running)
			{
				return;
			}

			update_status(server_status::stopping);
			if (_update_thread.joinable())
			{
				write_log(LG_DEBUG, "DEQUE", "Waiting for terminating of update thread...");

				_update_thread.join();

				write_log(LG_DEBUG, "DEQUE", "Update thread was terminated");
			}
		}

		// --------------------

		inline bool is_empty()
		{
			return _tasks_in_queue_count == 0 && _run_tasks_count == 0;
		}

		// --------------------

		bool can_run_task()
		{
			return _run_tasks_count < _max_tasks;
		}

		// --------------------

		void update_status(server_status status)
		{
			std::stringstream ss;
			ss << "Changing deque status from " << server_string(_status) << " to " << server_string(status);
			write_log(LG_DEBUG, "DEQUE", ss.str());
			_status = status;
		}

		// --------------------

	public:

		task_deque(size_t size, time_t timeout, void(*callback) (int, ReturnType, void*), void* args) :
			_max_tasks(size),
			_timeout(timeout),
			_args(args)
		{	
			_p_callback = callback;
			_p_buffer = new std::list<std::shared_ptr<task<ReturnType, Args>>>();
			_status = server_status::stopped;
		}

		// --------------------

		~task_deque()
		{
			_mutex.lock();
			_p_buffer->clear();
			_mutex.unlock();

			update_status(server_status::undefinded);
		}

		// --------------------

		task<ReturnType, Args>* new_task(task<ReturnType, Args> * p_task)
		{
			std::shared_ptr<task<ReturnType, Args>> shrp_task(p_task);

			_mutex.lock();
			_p_buffer->push_back(shrp_task);
			_tasks_in_queue_count++;
			_mutex.unlock();

			if (_status == server_status::stopped)
			{
				run();
			}

			return shrp_task.get();
		}

		// --------------------

		void terminate()
		{
			stop();

			update_status(server_status::terminating);
			this->~task_deque();
		}

		// --------------------

	};

	/// ================================================================================================
}
#endif // TASK_DEQUE_H

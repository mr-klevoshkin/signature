#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <functional>

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING

#include <experimental/filesystem>

#include "logger.h"
#include "task_deque.h"
#include "runner.h"

using namespace std;
using namespace logging;
using namespace taskking;
using namespace running;

// ====================================== GLOBAL TYPES =======================================================

using input_t = vector<char>;
using output_t = size_t;

typedef struct
{
	size_t block_size_b;
	string input_file_path;
	string output_file_path;

	size_t max_tasks;
	size_t max_threads;
	time_t update_timeout_ms;

	bool use_log;
	string log_file_path;

	unique_ptr<task_deque<output_t, input_t>> p_deque;
	unique_ptr<map<int, string>> p_map;
	atomic_int map_size;
} globals_t;

// ====================================== CALLBACKS AND FUNCTIONS =======================================================

const void callback(const int task_id, const output_t value, const void* args)
{
	globals_t* p_globals = (globals_t*)args;

	write_log(LG_DEBUG, "CALLBACK", "Task id: " + to_string(task_id));

	(*p_globals->p_map)[task_id] = to_string(value);
	p_globals->map_size++;
}

// --------------------

output_t calc_hash(const unique_ptr<input_t> input)
{
	hash<string> hash_fn;

	return hash_fn(input->data());
}

// --------------------

void stop_and_exit(shared_ptr<globals_t> p_globals, const int return_code)
{
	write_log(LG_INFO, "MAIN", "Termination...");

	if (p_globals)
	{
		if (p_globals->p_deque)
			p_globals->p_deque->terminate();
		
		if (p_globals->p_map)
			p_globals->p_map->clear();
	}
	
	exit(return_code);
}

// --------------------

void stop_and_exit_handler(const int return_code, const string error_message, void* ptr)
{
	write_log(LG_WARNING, "MAIN", "Safe invoking is broken");

	if (error_message != "")
	{
		write_log(LG_WARNING, "RUNNER", error_message);
	}

	globals_t* p_globals = (globals_t*)ptr;

	if (p_globals)
	{
		if (p_globals->p_deque)
			p_globals->p_deque->terminate();

		if (p_globals->p_map)
			p_globals->p_map->clear();
	}

	exit(return_code);
}

// --------------------

bool try_get_buf(const size_t buffer_size, unique_ptr<input_t> & p_buf)
{
	try
	{
		p_buf = unique_ptr<input_t>(new input_t(buffer_size));
		return true;
	}
	catch(...)
	{
		return false;
	}
}

// --------------------

bool check_block_size(const size_t block_size_in_bytes, string & error_message)
{
	if (block_size_in_bytes <= 0)
	{
		error_message = "Block size have to be more than 0";
		return false;
	}

	unique_ptr<input_t> tmp;
	if (!try_get_buf(block_size_in_bytes, tmp))
	{
		error_message = "Unable to allocate requested memory";
		return false;
	}

	return true;
}

// --------------------

void close_filestream(ifstream filestream)
{
	if (filestream && filestream.is_open())
	{
		filestream.close();
	}
}

// ========================================= ARCHITECTURE =======================================================

int finalize(shared_ptr<globals_t> p_globals)
{
	ofstream filestream(p_globals->output_file_path, ios::binary);
	if (!filestream.is_open())
	{
		return -1;
	}

	for_each(p_globals->p_map->begin(), p_globals->p_map->end(), [&filestream](pair<int, string> pair)
		{
			filestream.write(pair.second.data(), pair.second.length());
		});

	filestream.close();

	return 0;
}

// --------------------

int start(shared_ptr<globals_t> p_globals)
{
	ifstream filestream(p_globals->input_file_path, ios::binary);
	if (!filestream.is_open())
	{
		return -1;
	}
	int current_part = 0;
	size_t buffer_size = p_globals->block_size_b;
	unique_ptr<input_t> p_buf;

	try
	{
		while (!filestream.eof())
		{
			if (try_get_buf(buffer_size, p_buf))
			{
				filestream.read(p_buf->data(), p_globals->block_size_b);

				if (filestream.eof())
				{
					int last_part_size = filestream.gcount();
					if (last_part_size != p_globals->block_size_b)
					{
						fill(p_buf->begin() + last_part_size, p_buf->end(), 0);
					}
				}

				p_globals->p_deque->new_task(new task<output_t, input_t>(current_part++, calc_hash, move(p_buf)));
			}
		}
		close_filestream(move(filestream));
	}
	catch (exception e)
	{
		close_filestream(move(filestream));
		throw e;
	}
	
	while (p_globals->map_size != current_part)
	{
		this_thread::sleep_for(chrono::milliseconds(p_globals->update_timeout_ms));
	}

	return 0;
}

// --------------------

int init(const int argc, const char* args[], shared_ptr<globals_t> & p_globals)
{
	write_log(LG_INFO, "MAIN", "Initialization...");

	// input validation
	string input_file_path = args[1];
	if (!experimental::filesystem::exists(input_file_path))
	{
		write_log(LG_ERROR, "MAIN", "Input file " + input_file_path + " does not exist");
		return -1;
	}

	int block_size_in_bytes = 1000000; // default value is 1 Mb = 1 000 000 b
	if (argc == 4)
	{
		string arg = args[3];
		try
		{
			block_size_in_bytes = stoi(arg) * 1000000; // input in Mb so convert to bytes			
		}
		catch(invalid_argument e)
		{
			write_log(LG_ERROR, "MAIN", "Incorrect block size : " + string(e.what()));
			return -1;
		}
	}

	string error_message;
	if (!check_block_size(block_size_in_bytes, error_message))
	{
		write_log(LG_ERROR, "MAIN", error_message);
		return -1;
	}
	
	p_globals = unique_ptr<globals_t>(new globals_t());
	p_globals->input_file_path = input_file_path;
	p_globals->output_file_path = args[2];
	p_globals->block_size_b = block_size_in_bytes;
	p_globals->max_threads = 500;
	p_globals->update_timeout_ms = 300;
	p_globals->p_map = unique_ptr<map<int, string>>(new map<int, string>());
	p_globals->p_deque = unique_ptr<task_deque<output_t, input_t>>(
		new task_deque<output_t, input_t>(p_globals->max_threads, p_globals->update_timeout_ms, callback, (void*)(p_globals.get())));
	
	write_log(LG_SUCCESS, "MAIN", "Initialized successfully");
	return 0;
}

// ======================================== ENTER POINT ========================================================
static void print_usage_and_exit()
{
	cout <<
		"Usage: ./signature input_file_path output_file_path <block_size>" << endl << endl <<
		"	input_file_path		Path to the file used for creating a signature" << endl <<
		"	output_file_path	Path to the file that will contain the created signature" << endl <<
		"	block_size			Size of the block (in Mb) used for splitting input file" << endl <<
		"						Default value is 1 Mb." << endl << endl;
	exit(EXIT_SUCCESS);
}

int main(int argc, const char* argv[])
{
	/// 1. Input validation
	if (argc < 3 || argc > 4)
		print_usage_and_exit();

	/// 2. Initialize global parameters
	shared_ptr<globals_t> p_globals;

	if (init(argc, argv, p_globals) < 0)
		stop_and_exit(move(p_globals), EXIT_FAILURE);

	/// 3. Declaration of the main process
	const auto process = [&]() {
		
		/// Start
		if (start(p_globals) < 0)
			stop_and_exit(move(p_globals), EXIT_FAILURE);

		/// Finalize
		if (finalize(p_globals) < 0)
			stop_and_exit(move(p_globals), EXIT_FAILURE);

		/// Stop and exit
		stop_and_exit(move(p_globals), EXIT_SUCCESS);
	};

	/// 4. Init system signals parser and safe invoke
	runner::safe_invoke(process, stop_and_exit_handler, (void*)p_globals.get());
}

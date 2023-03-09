#pragma once

#include <memory>
#include <vector>
#include <condition_variable>
#include <thread>
#include <mutex>

struct Task
{
	virtual ~Task() = default;
	virtual void operator()() noexcept = 0;
	
	inline static auto mutex = std::mutex();
	inline static auto condition_variable = std::condition_variable();
	inline static auto tasks = std::vector<std::unique_ptr<Task>>();
	inline static bool should_terminate = false;
	
	static void emplace_task(std::unique_ptr<Task> task)
	{
		{
			auto lg = std::lock_guard(mutex);
			tasks.emplace_back(std::move(task));
		}
		
		condition_variable.notify_one();
	}
	
	template<typename Function>
	static void emplace(Function&& function)
	{
		struct Function_task final : Task, Function
		{
			Function_task(Function&& function)
				:
				Function(std::forward<Function>(function))
			{
			}
			
			void operator()() noexcept override
			{
				static_cast<Function&>(*this)();
			}
		};
		
		emplace_task(std::make_unique<Function_task>(std::forward<Function>(function)));
	}
	
	inline static struct Jthread : std::thread
	{
		~Jthread()
		{
			emplace_task(nullptr);
			join();
		}
		
		static void loop() noexcept
		{
			tasks.reserve(64);
			
			while (true)
			{
				auto task = std::unique_ptr<Task>();
				
				{
					auto ul = std::unique_lock(mutex);
					
					if (tasks.empty())
					{
						if (should_terminate)
						{
							break;
						}
						
						condition_variable.wait(ul, []() -> bool
						{
							return not tasks.empty();
						});
					}
					
					task = std::move(tasks.back());
					tasks.pop_back();
				}
				
				if (not task)
				{
					should_terminate = true;
				}
				else
				{
					(*task)();
				}
			}
		}
		
		Jthread()
			:
			std::thread(&loop)
		{
		}
	}
	thread;
};

#ifndef __writelock_hpp_defined
#define __writelock_hpp_defined

#include "ThreadSafe.hpp"

namespace Lock
{
	template<class T, class mutex_t = default_mutex>
	class WriteLock
	{
		ThreadSafe<T, mutex_t> *proxy;
		T *object;
		using _MyT = WriteLock<T, mutex_t>;
	public:
		WriteLock(ThreadSafe<T, mutex_t> &proxy) : proxy(&proxy), object(nullptr) { proxy.lock_write(this); object = reinterpret_cast<T*>(&reinterpret_cast<T&>(proxy.write(this))); }
		WriteLock(const _MyT &) = delete;
		WriteLock(_MyT && move) : proxy(move.proxy), object(move.object)
		{
			if(proxy)
				proxy->move_write_lock(&move, this);
			move.proxy = nullptr;
			move.object = nullptr;
		}
		~WriteLock()
		{
			if(proxy)
				proxy->unlock_write(this);
			proxy = nullptr;
			object = nullptr;
		}

		_MyT &operator=(const _MyT &) = delete;
		_MyT &operator=(_MyT && move)
		{
			if(this == &move)
				return *this;

			if(proxy)
				proxy->unlock_write(this);

			proxy = move.proxy;
			object = move.object;

			if(proxy)
				proxy->move_write_lock(&move, this);

			move.proxy = nullptr;
			move.object = nullptr;

			return *this;
		}

		inline T* operator->() const { return object; }
		inline T& operator*() const { return *object; }
	};
}
#endif
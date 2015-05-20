#ifndef __readlock_hpp_defined
#define __readlock_hpp_defined

#include "ThreadSafe.hpp"

namespace Lock
{
	template<class T, class mutex_t = default_mutex>
	class ReadLock
	{
		ThreadSafe<T, mutex_t> *proxy;
		const T* object;
		using _MyT = ReadLock<T, mutex_t>;
	public:
		ReadLock(ThreadSafe<T, mutex_t> &proxy) : proxy(&proxy), object(nullptr) { proxy.lock_read(); object = reinterpret_cast<const T*>(&reinterpret_cast<const char &>(proxy.read())); }
		ReadLock(const _MyT &other) : proxy(other.proxy), object(other.object) { if(proxy) proxy->lock_read(); }
		ReadLock(_MyT &&move) : proxy(move.proxy), object(move.object)
		{
			move.proxy = nullptr;
			move.object = nullptr;
		}
		~ReadLock()
		{
			if(proxy)
				proxy->unlock_read();
			proxy = nullptr;
			object = nullptr;
		}
		_MyT &operator=(const _MyT &other)
		{
			if(&other == this || proxy == other.proxy)
				return *this;

			if(proxy)
				proxy->unlock_read();

			proxy = other.proxy;

			if(proxy)
				proxy->lock_read();

			return *this;
		}

		const T* operator->() const { return object; }
		const T& operator*() const { return *object; }
	};
}

#endif
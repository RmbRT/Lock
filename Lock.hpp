#ifndef __lock_lock_hpp_defined
#define __lock_lock_hpp_defined

#include <mutex>
#include <type_traits>

namespace Lock
{

	struct bad_read_unlock { };
	struct bad_write_unlock { };
	struct bad_write { };
	struct bad_read { };
	struct bad_move_write_lock { };
	struct bad_thread_safe_move { };
	struct bad_thread_safe_destruct { };

	typedef std::mutex default_mutex;

	template<class T, class mutex_t>
	class WriteLock;
	template<class T, class mutex_t>
	class ReadLock;

	template<class T, class mutex_t = default_mutex>
	class ThreadSafe
	{
		friend class WriteLock<T,mutex_t>;
		friend class ReadLock<T,mutex_t>;
		using _MyT = ThreadSafe<T, mutex_t>;
		using _WriteLock = WriteLock<T, mutex_t>;
		using _ReadLock = ReadLock<T, mutex_t>;


		mutex_t _mutex;

		_WriteLock * _write_lock;
		unsigned _read_lock;

		T obj;

	public:
		template<class ...Args>
		ThreadSafe(Args&&... args) : _mutex(), _write_lock(nullptr), _read_lock(0), obj(std::forward<Args>(args)...) { }

		ThreadSafe(_MyT && move) throw (bad_thread_safe_move)
			: _mutex(), _write_lock(move._write_lock), _read_lock(move._read_lock), obj(std::move(move.obj))
		{
			if(_write_lock || _read_lock)
				throw bad_thread_safe_move();
		}

		~ThreadSafe() throw (bad_thread_safe_destruct)
		{
			if(_write_lock || _read_lock)
				throw bad_thread_safe_destruct();
		}

		template<class ...Args>
		_MyT &operator=(Args... args) = delete;

		ThreadSafe(const _MyT &) = delete;

		inline _WriteLock writeLock() { return _WriteLock(*this); }
		inline _ReadLock readLock() { return _ReadLock(*this); }

		inline const T &read_unsafe()
		{
			return obj;
		}
		inline T &write_unsafe()
		{
			return obj;
		}

	private:
		void lock_write(_WriteLock * key)
		{
			for(;;)
			{
				std::lock_guard<mutex_t> lock(_mutex);
			
				if(!_write_lock && !_read_lock)
				{
					_write_lock = key;
					return;
				}
			}
		}

		void move_write_lock(_WriteLock * oldKey, _WriteLock * newKey) throw (bad_move_write_lock)
		{
			std::lock_guard<mutex_t> lock(_mutex);
			if(!_write_lock || _write_lock != oldKey || !newKey)
				throw bad_move_write_lock();
			else
				_write_lock = newKey;
		}

		void unlock_write(_WriteLock * key) throw (bad_write_unlock)
		{
			std::lock_guard<mutex_t> lock(_mutex);

			if(!_write_lock || _write_lock != key)
				throw bad_write_unlock();

			_write_lock = nullptr;
		}

		void lock_read()
		{
			for(;;)
			{
				std::lock_guard<mutex_t> lock(_mutex);
		
				if(!_write_lock)
				{
					_read_lock++;
					return;
				}
			}
		}

		void unlock_read() throw (bad_read_unlock)
		{
			for(;;)
			{
				std::lock_guard<mutex_t> lock(_mutex);

				if(!_write_lock)
					if(!_read_lock)
						throw bad_read_unlock();
					else
					{
						_read_lock--;
						return;
					}
			}
		}

		T &write(WriteLock<T,mutex_t> * key)
		{
			std::lock_guard<mutex_t> lock(_mutex);
			// needs write lock and the right key to write.
			if(!_read_lock && (_write_lock? _write_lock == key : true))
				return obj;
			else
				throw bad_write();
		}

		const T &read()
		{
			std::lock_guard<mutex_t> lock(_mutex);
			if(!_write_lock)
				return obj;
		}
	};

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
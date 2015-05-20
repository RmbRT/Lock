#ifndef __proxy_hpp_defined
#define __proxy_hpp_defined

#include <stack>
#include <mutex>
#include <type_traits>

namespace Lock
{

	struct bad_read_unlock { };
	struct bad_write_unlock { };
	struct bad_write { };
	struct bad_read { };
	struct bad_move_write_lock { };
	struct bad_thread_safe_move{ };

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
}


#endif
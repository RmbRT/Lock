#ifndef __lock_lock_hpp_defined
#define __lock_lock_hpp_defined

#include <mutex>
#include <type_traits>
#include <cassert>

namespace Lock
{

	struct bad_read_unlock { };
	struct bad_write_unlock { };
	struct bad_write { };
	struct bad_read { };
	struct bad_move_write_lock { };
	struct bad_thread_safe_move { };
	struct bad_thread_safe_destruct { };

	template<class T>
	class WriteLock;
	template<class T>
	class ReadLock;
	template<class T>
	class ThreadSafe;
	
	namespace helper
	{
		template<class T>
		struct read_lock_pair
		{
			ReadLock<T> &lock;
			ThreadSafe<T> &thread_safe;
		};

		template<class T>
		struct write_lock_pair
		{
			WriteLock<T> &lock;
			ThreadSafe<T> &thread_safe;
		};

		inline void destructor(void) { }
		template<class T, class ...Targs>
		inline void destructor(T &obj, Targs&... args) { obj.~T(); destructor(args...); }
		inline bool try_read_lock(void) { return true; }
		inline bool try_write_lock(void) { return true; }
	}

	template<class T>
	/*Use this function to pass a (ReadLock, ThreadSafe) pair to the locking functions try_read_lock and multi_read_lock.*/
	inline helper::read_lock_pair<T> pair(ReadLock<T> &lock, ThreadSafe<T> &thread_safe) { return { lock, thread_safe }; }

	template<class T>
	/*Use this function to pass a (WriteLock, ThreadSafe) pair to the locking functions try_write_lock and multi_write_lock.*/
	inline helper::write_lock_pair<T> pair(WriteLock<T> &lock, ThreadSafe<T> &thread_safe) { return { lock, thread_safe }; }

	template<class T, class ...Targs>
	/*Tries to lock all the passed ThreadSafe with their paired ReadLock. Returns true, if all locks were successful.
	See ThreadSafe::try_lock_read.*/
	inline bool try_read_lock(helper::read_lock_pair<T> &arg, helper::read_lock_pair<Targs>&... args)
	{
		using namespace helper;
		return arg.thread_safe.try_lock_read(arg.lock) && try_read_lock(args...);
	}

	template<class ...T>
	/*Locks multiple thread safe objects for reading, and sets the passed read locks to their corresponding thread safe object.
	void multi_read_lock(helper::read_lock_pair<T>&... args)
	{	using namespace helper;
		// try to lock all thread safe objects.
		while(!try_read_lock(args...))
			// not all resources could be locked. Release all locks, try again.
			destructor(args.lock...);
	}
	template<class T, class ...Targs>
	/*Tries to lock all the passed ThreadSafe with their paired WriteLock. Returns true, if all locks were successful.
	See ThreadSafe::try_lock_write.*/
	inline bool try_write_lock(helper::write_lock_pair<T> &arg, helper::write_lock_pair<Targs>&... args)
	{	using namespace helper;
		return arg.thread_safe.try_lock_write(arg.lock) && try_write_lock(args...);
	}

	template<class ...T>
	/*Locks multiple thread safe objects for writing, and sets the passed write locks to their corrseponding thread safe object.
	void multi_write_lock(helper::write_lock_pair<T>&... args)
	{	using namespace helper;
		// try to lock all thread safe objects.
		while(!try_write_lock(args...))
			// not all resources could be locked. Release all locks, try again.
			destructor(args.lock...);
	}

	template<class T>
	/*Wrapper class for shared resources.
	Use in combination with ReadLock and WriteLock, as well as the multi_read_lock and multi_write_lock functions to ensure thread safety.
	Note that only one write lock may be attached to every shared resource at a time.
	A shared resource can be read locked multiple times at once. Only when all read locks are lifted, the shared resource is unlocked.
	While a shared resource is write locked, it can not be read locked.
	While a shared resource is read locked, it can not be write locked.*/
	class ThreadSafe
	{
		friend class WriteLock<T>;
		friend class ReadLock<T>;
	public:
		using _MyT = ThreadSafe<T>;
		typedef WriteLock<T> _WriteLock;
		typedef ReadLock<T> _ReadLock;
	private:

		std::mutex _mutex;

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

		bool try_lock_write(_WriteLock & out)
		{
			// avoid re-locking locked writelocks to the same proxy, since that must be some mistake on the user side.
			assert(out.proxy != this);

			std::lock_guard<std::mutex> lock(_mutex);
			if(!_write_lock && !_read_lock)
			{
				_write_lock = &out;
				// do not use assignment here, as it would try to lock our mutex again!

				// release old lock, if any.
				out.~WriteLock();
				// assign new write lock.
				new(&out) _WriteLock(this, reinterpret_cast<T*>(&reinterpret_cast<char &>(obj)));
				return true;
			}
			else
				return false;
		}

		bool try_lock_read(_ReadLock &out)
		{
			// avoid re-locking locked readlocks to the same proxy, since that must be some mistake on the user side.
			assert(out.proxy != this);

			std::lock_guard<std::mutex> lock(_mutex);
			if(!_write_lock)
			{
				_read_lock++;
				// do not use assignment here, as it would try to lock our mutex again!

				// release old read lock, if any.
				out.~ReadLock();
				// assign new read lock.
				new (&out) _ReadLock(this, reinterpret_cast<const T*>(&reinterpret_cast<const char &>(obj)));
				return true;
			}
			else
				return false;
		}


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
				std::lock_guard<std::mutex> lock(_mutex);
			
				if(!_write_lock && !_read_lock)
				{
					_write_lock = key;
					return;
				}
			}
		}

		void move_write_lock(_WriteLock * oldKey, _WriteLock * newKey) throw (bad_move_write_lock)
		{
			std::lock_guard<std::mutex> lock(_mutex);
			if(!_write_lock || _write_lock != oldKey || !newKey)
				throw bad_move_write_lock();
			else
				_write_lock = newKey;
		}

		void unlock_write(_WriteLock * key) throw (bad_write_unlock)
		{
			std::lock_guard<std::mutex> lock(_mutex);

			if(!_write_lock || _write_lock != key)
				throw bad_write_unlock();

			_write_lock = nullptr;
		}

		void lock_read()
		{
			for(;;)
			{
				std::lock_guard<std::mutex> lock(_mutex);
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
				std::lock_guard<std::mutex> lock(_mutex);

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

		T &write(WriteLock<T> * key)
		{
			std::lock_guard<std::mutex> lock(_mutex);
			// needs write lock and the right key to write.
			if(!_read_lock && (_write_lock? _write_lock == key : true))
				return obj;
			else
				throw bad_write();
		}

		const T &read()
		{
			std::lock_guard<std::mutex> lock(_mutex);
			if(!_write_lock)
				return obj;
		}
	};

	template<class T>
	/*Scoped read lock class. See the descriptions for ThreadSafe.*/
	class ReadLock
	{
		friend class ThreadSafe<T>;
		ThreadSafe<T> *proxy;
		const T* object;
		using _MyT = ReadLock<T>;

		ReadLock(ThreadSafe<T> *proxy, const T *object) : proxy(proxy), object(object) { }
	public:
		ReadLock() : proxy(nullptr), object(nullptr) { }
		/*Blocks the current thread until a lock on the resource could be optained.*/
		ReadLock(ThreadSafe<T> &proxy) : proxy(&proxy), object(nullptr) { proxy.lock_read(); object = reinterpret_cast<const T*>(&reinterpret_cast<const char &>(proxy.read())); }
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

		inline const T* operator->() const { return object; }
		inline const T& operator*() const { return *object; }
		inline bool locked() const { return proxy != nullptr; }
		
	};

	template<class T>
	/*Scoped write lock class. See the descriptions for ThreadSafe.*/
	class WriteLock
	{
		friend class ThreadSafe<T>;
		ThreadSafe<T> *proxy;
		T *object;
		using _MyT = WriteLock<T>;

		WriteLock(ThreadSafe<T> *proxy, T * object) : proxy(proxy), object(object) { }
	public:
		WriteLock() : proxy(nullptr), object(nullptr) { }
		/*Blocks the current thread until a lock on the resource could be optained.*/
		WriteLock(ThreadSafe<T> &proxy) : proxy(&proxy), object(nullptr) { proxy.lock_write(this); object = reinterpret_cast<T*>(&reinterpret_cast<T&>(proxy.write(this))); }
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

		inline bool locked() const { return proxy != nullptr; }
	};
}


#endif
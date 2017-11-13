#ifndef __lock_lock_hpp_defined
#define __lock_lock_hpp_defined

#include <mutex>
#include <type_traits>
#include <cassert>
#include <thread>
#include <memory>
#include <atomic>
#include <random>

namespace lock
{
	template<class T>
	class WriteLock;
	template<class T>
	class ReadLock;
	template<class T>
	class ThreadSafe;

	template<class T>
	/** Binds a read lock handle to a resource. */
	struct ReadLockPair
	{
		/** The lock to lock `thread_safe`. */
		ReadLock<T> &lock;
		/** The thread safe resource to be locked. */
		ThreadSafe<T> &thread_safe;

		/** Creates a read lock pair.
		@param[in] lock:
			The lock to lock `thread_safe`.
		@param[in] thread_safe:
			The thread safe resource to be locked. */
		ReadLockPair(
			ReadLock<T> &lock,
			ThreadSafe<T> &thread_safe);
	};

	template<class T>
	/** Binds a write lock handle to a resource. */
	struct WriteLockPair
	{
		/** The lock to lock `thread_safe`. */
		WriteLock<T> &lock;
		/** The thread safe resource to be locked. */
		ThreadSafe<T> &thread_safe;

		/** Creates a write lock pair.
		@param[in] lock:
			The lock to lock `thread_safe`.
		@param[in] thread_safe:
			The thread safe resource to be locked. */
		WriteLockPair(
			WriteLock<T> &lock,
			ThreadSafe<T> &thread_safe);
	};

	/** Helper namespace with functions and data types that are only of internal use. */
	namespace helper
	{
		struct bad_read_unlock { };
		struct bad_write_unlock { };
		struct bad_write { };
		struct bad_read { };
		struct bad_move_write_lock { };
		struct bad_thread_safe_move { };
		struct bad_thread_safe_destruct { };

		template<class ... Types>
		struct each_exists {};

		template<class ...T>
		/** SFINAE type to check for iterators. */
		struct each_iterator
		{
			typedef each_exists<typename std::iterator_traits<T>::iterator_category...> type;
		};

		template<class T>
		struct is_lock_pair { };
		template<class T>
		struct is_lock_pair<WriteLockPair<T>>
		{
			typedef WriteLockPair<T> type;
		};
		template<class T>
		struct is_lock_pair<ReadLockPair<T>>
		{
			typedef ReadLockPair<T> type;
		};

		template<class ...T>
		struct each_lock_pair
		{
			typedef each_exists<typename is_lock_pair<T>::type...> type;
		};

		template<class ...T>
		struct each_lock_pair_iterator
		{
			typedef typename each_lock_pair<typename std::iterator_traits<T>::value_type...>::type type;
		};
	}

	template<class T>
	/** A range between two iterators. */
	class Range
	{
		/** The beginning of the range. */
		T m_begin;
		/** The end of the range. */
		T m_end;
	public:
		Range() = default;
		/** Creates the range [`begin`, `end`).
		@param[in] begin:
			The beginning of the range.
		@param[in] end:
			The end of the range. */
		Range(
			T begin,
			T end);

		/** Returns the beginning of the range. */
		inline T const& begin() const;
		/** Returns the end of the range. */
		inline T const& end() const;
	};

	template<class T>
	/** Use this function to pass a (`ReadLock`, `ThreadSafe`) pair to the locking functions `lock::multi_lock` and `lock::multi_read_lock`.
	@param[in] lock:
		The lock to lock `thread_safe`.
	@param[in] thread_safe:
		The thread safe resource to be locked.
	@return
		The pair (`lock`, `thread_safe`). */
	inline ReadLockPair<T> pair(
		ReadLock<T> &lock,
		ThreadSafe<T> &thread_safe);

	template<class T>
	/** Use this function to pass a (`WriteLock`, `ThreadSafe`) pair to the locking functions `lock::multi_lock` and `lock::multi_write_lock`.
	@param[in] lock:
		The lock to lock `thread_safe`.
	@param[in] thread_safe:
		The thread safe resource to be locked.
	@return
		The pair (`lock`, `thread_safe`). */
	inline WriteLockPair<T> pair(
		WriteLock<T> &lock,
		ThreadSafe<T> &thread_safe);

	template<class T, class = typename helper::each_lock_pair_iterator<T>::type>
	/** Creates a range denoted by a begin and end iterator.
	@param[in] begin:
		The start of the range.
	@param[in] end:
		The end of the range.
	@return
		A Range describing `[begin, end)`. */
	inline Range<T> range(
		T begin,
		T end);

	template<class T>
	/** Use this function to acquire a write lock object for the given thread safe resource.
		This function will block the current thread until a lock could be acquired.
	@param[in,out] thread_safe:
		The thread safe resource to lock.
	@return
		A write lock handle to the thread safe resource. */
	inline WriteLock<T> write_lock(
		ThreadSafe<T> &thread_safe);

	template<class T>
	/** Use this function to acquire a read lock object for the given thread safe resource.
		This function will block the current thread until a lock could be acquired.
	@param[in,out] thread_safe:
		The thread safe resource to lock.
	@return
		A read lock handle to the thread safe resource. */
	inline ReadLock<T> read_lock(
		ThreadSafe<T> &thread_safe);


	template<class ...InputIterator,
		class = typename helper::each_lock_pair_iterator<InputIterator...>::type>
	/** Locks ranges of lock pairs.
		This function prevents deadlocks and livelocks.
	@tparam InputIterator:
		Must be an iterator type over either ReadLockPair or WriteLockPair.
	@param[in] ranges:
		The ranges of lock pairs to lock. */
	inline void range_lock(
		Range<InputIterator>... ranges);

	template<class ...T>
	/** Locks multiple thread safe objects for writing or reading.
		This function releases all locks and tries to re-lock all locks in case one or more resources could not be locked, to prevent dead locks, and retries.
	@param[in,out] pairs:
		A mix of resource and read / write lock pairs to lock. These can be acquired using the `lock::pair()` function. */
	inline void multi_lock(
		T&&... pairs);

	template<class ...T>
	/** Locks multiple thread safe objects for reading.
		This function releases all locks and tries to re-lock all locks in case one or more resources could not be locked, to prevent dead locks, and retries. Note: If you have to read lock as well as write lock objects for a function / operation, use lock::multi_lock instead.
	@param[in] pairs:
		The resource and read lock pairs to lock. These can be acquired using the `lock::pair()` function. */
	inline void multi_read_lock(
		ReadLockPair<T>... pairs);

	template<class ...T>
	/** Locks multiple thread safe objects for writing.
		This function releases all locks and tries to re-lock all locks in case one or more resources could not be locked, to prevent dead locks, and retries. Note: If you have to read lock as well as write lock objects for a function / operation, use lock::multi_lock instead.
	@param[in] pairs:
		The resource and write_lock pairs to lock. These can be acquired using the `lock::pair()` function. */
	inline void multi_write_lock(
		WriteLockPair<T>... pairs);

	/** Tickets used to reserve a thread safe resource. */
	typedef std::uint16_t ticket_t;


	template<class T>
	/** Wrapper class for shared resources.
		Use in combination with ReadLock and WriteLock, as well as the multi_lock function to ensure thread safety and prevent dead locks. To be explicit about only read locking / write locking, use multi_read_lock and multi_write_lock. A function / operation should ony have one lock call to acquire its locks. This prevents dead locks / incomplete locking of needed resources. Note that only one write lock may be attached to every shared resource at a time. A shared resource can be read locked multiple times at once. The shared resource is unlocked only after all read locks are released. While a shared resource is write locked, it can not be read locked. While a shared resource is read locked, it cannot be write locked. */
	class ThreadSafe
	{
		friend class WriteLock<T>;
		friend class ReadLock<T>;

		static struct Authorised { } const authorised;

		/** The thread safe object. */
		T m_object;

		/** The mutex that protects the object. */
		std::mutex m_mutex;

		/** Whether the thread safe object is write locked. */
		bool m_write_lock;
		/** The current read lock count. */
		std::atomic<std::size_t> m_read_locks;

		/** The ticket with the highest priority. */
		ticket_t m_priority;
		/** Whether and, if, by whom, the thread safe object is reserved for ownership. */
		std::thread::id m_reserved_by;

	public:
		template<class ...Args>
		/** Creates a thread safe object with the given arguments.
		@param[in] args:
			The arguments used to construct the object. */
		ThreadSafe(
			Args&&... args);

		/** Creates a thread safe object by moving.
			The source object must not be locked.
		@param[in,out] move:
			The thread safe object to move. */
		ThreadSafe(
			ThreadSafe<T> && move);

		/** Destroys a thread safe object.
			The object must not be locked. */
		~ThreadSafe();

		ThreadSafe<T> &operator=(
			ThreadSafe<T> const&) = delete;

		ThreadSafe(
			ThreadSafe<T> const&) = delete;

		/** Aquires a write lock.
			This function blocks until a write lock is acquired. */
		WriteLock<T> write();
		/** Attempts to aquire a write lock.
			May fail, but does not block. */
		WriteLock<T> try_write();


		/** Aquires a read lock.
			This function blocks until a read lock is acquired. */
		ReadLock<T> read();
		/** Attempts to acquire a read lock.
			May fail, but does not block. */
		ReadLock<T> try_read();

		/** Tries to reserve the thread safe object. */
		inline void reserve(
			ticket_t priority);
		/** Returns whether the thread safe object is reserved by any thread. */
		inline bool reserved() const;

	private:
		/** Removes the current reservation, if exists. */
		inline void unreserve();
		/** Determines whether the current executing thread can claim the thread safe object. */
		bool thread_can_claim();
		/** Can be called within a context where the mutex is already locked. */
		void reserve_locked(
			ticket_t priority);
	};

	template<class T>
	/** Scoped read lock class
		See the descriptions for ThreadSafe.*/
	class ReadLock
	{
		friend class ThreadSafe<T>;

		/** The proxy this lock is bound to. */
		ThreadSafe<T> * m_proxy;

		/** Creates a read lock bound to the given proxy.
		@param[in] proxy:
			The proxy that this lock is bound to. */
		inline ReadLock(
			ThreadSafe<T> & proxy,
			typename ThreadSafe<T>::Authorised);
	public:
		/** Creates an empty read lock. */
		inline ReadLock();
		/** Blocks the current thread until a lock on the resource could be optained.*/
		ReadLock(
			ThreadSafe<T> &proxy);
		/** Copies the read lock. */
		ReadLock(
			ReadLock<T> const& other);
		/** Moves the lock ownership from `move`.
		@param[in,out] move:
			The read lock to move. */
		ReadLock(
			ReadLock<T> &&move);
		/** Releases the read lock. */
		~ReadLock();

		/** Copies a read lock.
		@param[in] other:
			The lock to copy.
		@return
			A reference to `this`. */
		ReadLock<T> &operator=(
			ReadLock<T> const& other);
		/** Copies a read lock.
			Unlocks `this` if it is not empty.
		@param[in] other:
			The lock to copy.
		@return
			A reference to `this`. */
		ReadLock<T> &operator=(
			ReadLock<T> && other);

		/** Accesses the locked object.
		@return
			The locked object. */
		inline const T* operator->() const;
		/** Accesses the locked object.
		@return
			The locked object. */
		inline const T& operator*() const;
		/** Returns whether the read lock is bound to any proxy. */
		inline bool locked() const;
		/** Same as `locked()`. */
		inline operator bool() const;

		/** Locks a proxy. */
		inline void lock(
			ThreadSafe<T> &proxy);
		/** Tries to lock a proxy. */
		inline bool try_lock(
			ThreadSafe<T> &proxy);
		/** Releases the lock.
			The object must be locked. */
		inline void unlock();
	};

	template<class T>
	/*Scoped write lock class. See the descriptions for ThreadSafe.*/
	class WriteLock
	{
		friend class ThreadSafe<T>;
		ThreadSafe<T> * m_proxy;

		inline WriteLock(
			ThreadSafe<T> &proxy,
			typename ThreadSafe<T>::Authorised);
	public:
		inline WriteLock();
		/** Creates a write lock bound to the given proxy.
			Blocks until a lock is obtained.
		@param[in,out] proxy:
			The thread safe object to lock. */
		WriteLock(
			ThreadSafe<T> &proxy);
		/** Moves a write lock.
		@param[in,out] move:
			The write lock to move. */
		WriteLock(
			WriteLock<T> && move);
		/** Releases the write lock. */
		~WriteLock();
		/** Moves a write lock.
			Unlocks `this` if it is not empty.
		@param[in,out] move:
			The write lock to move.
		@return
			A reference to `this`. */
		WriteLock<T> &operator=(
			WriteLock<T> && move);

		inline T* operator->() const;
		inline T& operator*() const;
		inline bool locked() const;
		inline operator bool() const;

		inline void lock(
			ThreadSafe<T> &proxy);
		inline bool try_lock(
			ThreadSafe<T> &proxy);
		inline void unlock();
	};
}

#include "ThreadSafe.inl"
#include "ReadLock.inl"
#include "WriteLock.inl"


#endif

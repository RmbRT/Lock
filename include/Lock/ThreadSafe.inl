namespace lock
{
	namespace helper
	{
		inline void unlock(void) { }
		template<class T, class ...Targs>
		inline void unlock(ReadLock<T> &obj, Targs&... args)
		{
			if(obj.locked())
				obj.unlock();
			unlock(args...);
		}
		template<class T, class ...Targs>
		inline void unlock(WriteLock<T> &obj, Targs&... args)
		{
			if(obj.locked())
				obj.unlock();
			unlock(args...);
		}

		template<class T>
		inline bool try_lock(
			WriteLockPair<T> &pair)
		{
			return pair.lock.try_lock(pair.thread_safe);
		}
		template<class T>
		inline bool try_lock(
			ReadLockPair<T> &pair)
		{
			return pair.lock.try_lock(pair.thread_safe);
		}

		template<class T, class U, class ...Trest>
		inline bool try_lock(
			T &pair,
			U &rest0,
			Trest&...restN)
		{
			if(try_lock(pair))
			{
				if(!try_lock(rest0, restN...))
				{
					pair.lock.unlock();
					return false;
				}
				return true;
			} else
				return false;
		}

		template<class T>
		inline void reserve(
			ticket_t ticket,
			ThreadSafe<T> &ts)
		{
			ts.reserve(ticket);
		}

		template<class T, class ...Ts>
		inline void reserve(
			ticket_t ticket,
			ThreadSafe<T> &ts,
			ThreadSafe<Ts> &... rest)
		{
			reserve(ticket, rest...);
		}
	}


	template<class T>
	helper::ReadLockPair<T> pair(
		ReadLock<T> &lock,
		ThreadSafe<T> &thread_safe)
	{
		return { lock, thread_safe };
	}

	template<class T>
	helper::WriteLockPair<T> pair(
		WriteLock<T> &lock,
		ThreadSafe<T> &thread_safe)
	{
		return { lock, thread_safe };
	}

	template<class T>
	WriteLock<T> write_lock(
		ThreadSafe<T> &thread_safe)
	{
		return WriteLock<T>(thread_safe);
	}

	template<class T>
	ReadLock<T> read_lock(
		ThreadSafe<T> &thread_safe)
	{
		return ReadLock<T>(thread_safe);
	}

	template<class ...T>
	void multi_lock(T&&... pairs)
	{
		// first, try trivial locking (without reservations).
		if(helper::try_lock(pairs...))
			return;

		// create a ticket.
		std::random_device rd;
		std::uniform_int_distribution<ticket_t> dist(0, ~ticket_t(0));
		ticket_t const ticket = dist(rd);

		// now try locking via reservations.
		for(;; std::this_thread::yield())
		{
			// try reserving all resources.
			helper::reserve(ticket, pairs.thread_safe...);
			// try again to lock everything.
			if(helper::try_lock(pairs...))
				return;
		}
	}

	template<class ...T>
	void multi_read_lock(
		helper::ReadLockPair<T> ... pairs)
	{
		multi_lock(pairs...);
	}

	template<class ...T>
	void multi_write_lock(
		helper::WriteLockPair<T> ... pairs)
	{
		multi_lock(pairs...);
	}

	template<class T>
	template<class ...Args>
	ThreadSafe<T>::ThreadSafe(
		Args&&... args):
		m_mutex(),
		m_write_lock(false),
		m_read_locks(0),
		m_object(std::forward<Args>(args)...)
	{
	}

	template<class T>
	ThreadSafe<T>::ThreadSafe(
		ThreadSafe<T> && move):
		m_mutex(),
		m_write_lock(false),
		m_read_locks(0),
		m_object(std::move(move.m_object))
	{
		if(move.m_write_lock || move.m_read_locks.load(std::memory_order_relaxed))
			throw helper::bad_thread_safe_move();
	}

	template<class T>
	ThreadSafe<T>::~ThreadSafe()
	{
		assert(!m_write_lock && !m_read_locks);
	}

	template<class T>
	WriteLock<T> ThreadSafe<T>::write()
	{
		std::random_device rd;
		std::uniform_int_distribution<ticket_t> dist(0, ~ticket_t(0));
		ticket_t ticket = dist(rd);

		for(;; std::this_thread::yield())
		{
			std::lock_guard<std::mutex> lock(m_mutex);

			if(!m_write_lock
			&& !m_read_locks.load(std::memory_order_relaxed)
			&& thread_can_claim())
			{
				m_write_lock = true;
				unreserve();
				return WriteLock<T>(*this, authorised);
			}
			// only reserve after initial try failed.
			reserve_locked(ticket);
		}
	}

	template<class T>
	WriteLock<T> ThreadSafe<T>::try_write()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if(!m_write_lock
		&& !m_read_locks.load(std::memory_order_relaxed)
		&& thread_can_claim())
		{
			m_write_lock = true;
			unreserve();
			return WriteLock<T>(*this, authorised);
		}
		else
			return WriteLock<T>();
	}

	template<class T>
	ReadLock<T> ThreadSafe<T>::read()
	{
		std::random_device rd;
		std::uniform_int_distribution<ticket_t> dist(0, ~ticket_t(0));
		ticket_t ticket = dist(rd);

		for(;; std::this_thread::yield())
		{
			std::lock_guard<std::mutex> lock(m_mutex);

			if(!m_write_lock
			&& thread_can_claim())
			{
				m_read_locks.fetch_add(1, std::memory_order_relaxed);
				unreserve();
				return ReadLock<T>(*this, authorised);
			}

			// only reserve after initial try failed.
			reserve_locked(ticket);
		}
	}

	template<class T>
	ReadLock<T> ThreadSafe<T>::try_read()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if(!m_write_lock
		&& thread_can_claim())
		{
			m_read_locks.fetch_add(1, std::memory_order_relaxed);
			unreserve();
			return ReadLock<T>(*this, authorised);
		}
		else
			return ReadLock<T>();
	}

	template<class T>
	void ThreadSafe<T>::reserve(
		ticket_t priority)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		reserve_locked(priority);
	}

	template<class T>
	bool ThreadSafe<T>::reserved() const
	{
		return m_reserved_by != std::thread::id();
	}

	template<class T>
	void ThreadSafe<T>::unreserve()
	{
		return m_reserved_by = std::thread::id();
	}

	template<class T>
	bool ThreadSafe<T>::thread_can_claim()
	{
		return !reserved() || m_reserved_by == std::this_thread::get_id();
	}

	template<class T>
	void ThreadSafe<T>::reserve_locked(
		ticket_t priority)
	{
		if(!reserved() || priority > m_priority)
		{
			m_reserved_by = std::this_thread::get_id();
			m_priority = priority;
		} else if(m_priority == priority)
			if(m_reserved_by >= std::this_thread::get_id())
				m_reserved_by = std::this_thread::get_id();
	}
}
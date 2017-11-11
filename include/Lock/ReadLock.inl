namespace lock
{
	template<class T>
	ReadLock<T>::ReadLock(
		ThreadSafe<T> & proxy,
		typename ThreadSafe<T>::Authorised):
		m_proxy(&proxy)
	{
	}

	template<class T>
	ReadLock<T>::ReadLock():
		m_proxy(nullptr)
	{
	}

	template<class T>
	ReadLock<T>::ReadLock(
		ThreadSafe<T> &proxy):
		ReadLock(proxy.read())
	{
	}

	template<class T>
	ReadLock<T>::ReadLock(
		ReadLock<T> const& other):
		m_proxy(other.m_proxy)
	{
		if(m_proxy)
			++m_proxy->m_read_locks;
	}

	template<class T>
	ReadLock<T>::ReadLock(
		ReadLock<T> && move):
		m_proxy(move.m_proxy)
	{
		move.m_proxy = nullptr;
	}

	template<class T>
	ReadLock<T>::~ReadLock()
	{
		if(locked())
			--m_proxy->m_read_locks;
	}

	template<class T>
	ReadLock<T> &ReadLock<T>::operator=(
		ReadLock<T> const& other)
	{
		// unlock old proxy.
		if(m_proxy && m_proxy != other.m_proxy)
			--m_proxy->m_read_locks;

		m_proxy = other.m_proxy;

		// lock new proxy.
		if(other.m_proxy)
			++other.m_proxy->m_read_locks;

		return *this;
	}

	template<class T>
	ReadLock<T> &ReadLock<T>::operator=(
		ReadLock<T> && other)
	{
		if(this == &other)
			return *this;

		// unlock old proxy.
		if(m_proxy)
			--m_proxy->m_read_locks;

		m_proxy = other.m_proxy;
		other.m_proxy = nullptr;

		return *this;
	}

	template<class T>
	T const * ReadLock<T>::operator->() const
	{
		assert(locked()
			&& "Tried to access empty lock.");
		return std::addressof(m_proxy->m_object);
	}

	template<class T>
	T const& ReadLock<T>::operator*() const
	{
		assert(locked()
			&& "Tried to access empty lock.");

		return m_proxy->m_object;
	}

	template<class T>
	bool ReadLock<T>::locked() const
	{
		return m_proxy != nullptr;
	}

	template<class T>
	ReadLock<T>::operator bool() const
	{
		return locked();
	}


	template<class T>
	void ReadLock<T>::lock(
		ThreadSafe<T> &proxy)
	{
		*this = proxy.read();
	}

	template<class T>
	bool ReadLock<T>::try_lock(
		ThreadSafe<T> &proxy)
	{
		return *this = proxy.try_read();
	}

	template<class T>
	void ReadLock<T>::unlock()
	{
		assert(locked()
			&& "Tried to unlock empty lock.");

		--m_proxy->m_read_locks;
		m_proxy = nullptr;
	}
}
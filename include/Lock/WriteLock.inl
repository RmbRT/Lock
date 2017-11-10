namespace lock
{
	template<class T>
	WriteLock<T>::WriteLock(
		ThreadSafe<T> &proxy,
		typename ThreadSafe<T>::Authorised):
		m_proxy(&proxy)
	{
	}

	template<class T>
	WriteLock<T>::WriteLock():
		m_proxy(nullptr)
	{
	}

	template<class T>
	WriteLock<T>::WriteLock(
		ThreadSafe<T> &proxy):
		WriteLock(proxy.write())
	{
	}

	template<class T>
	WriteLock<T>::WriteLock(
		WriteLock<T> &&move):
		m_proxy(move.m_proxy)
	{
		move.m_proxy = nullptr;
	}

	template<class T>
	WriteLock<T>::~WriteLock()
	{
		if(locked())
			m_proxy->m_write_lock = false;
	}

	template<class T>
	WriteLock<T> &WriteLock<T>::operator=(
		WriteLock<T> &&move)
	{
		if(&move == this)
			return *this;

		if(locked())
			m_proxy->m_write_lock = false;

		m_proxy = move.m_proxy;
		move.m_proxy = nullptr;

		return *this;
	}

	template<class T>
	T * WriteLock<T>::operator->() const
	{
		assert(locked()
			&& "Tried to access empty lock.");

		return std::addressof(m_proxy->m_object);
	}

	template<class T>
	T & WriteLock<T>::operator*() const
	{
		assert(locked()
			&& "Tried to access empty lock.");

		return m_proxy->m_object;
	}

	template<class T>
	bool WriteLock<T>::locked() const
	{
		return m_proxy != nullptr;
	}

	template<class T>
	void WriteLock<T>::lock(
		ThreadSafe<T> & proxy)
	{
		*this = proxy.write();
	}

	template<class T>
	bool WriteLock<T>::try_lock(
		ThreadSafe<T> &proxy)
	{
		return *this = proxy.try_write();
	}

	template<class T>
	WriteLock<T>::operator bool() const
	{
		return locked();
	}

	template<class T>
	void WriteLock<T>::unlock()
	{
		assert(locked() &&
			"Tried to unlock empty lock.");

		m_proxy->m_write_lock = false;
		m_proxy = nullptr;
	}
}
# Lock
Lock is a thread safety library supporting locking shared resources.
## Features
* Scoped read / write locks
* Ability to lock multiple resources in one call, preventing only partially locking the resources and deadlocks.
* Livelock prevention.
* `lock::ThreadSafe` supports moving, but not copying.

## Important
If more than one resource has to be locked in a function, *always* do so in one call. For mixed type locks, use `lock::multi_lock()`. You may want to emphasize on the fact that you only want to acquire read locks / only write locks, and you can do so using `lock::multi_read_lock()` / `lock::multi_write_lock()`. If you create multiple locks using the constructor of the lock classes, then you could cause a dead lock, as you only partially lock the resources at once. To lock ranges of resources, use `lock::range_lock()`, where multiple ranges of `lock::ReadLockPair` and `lock::WriteLockPair` can be passed.
## Rules
* A shared resource may be locked for writing by only one `lock::WriteLock` instance at a time.
* A shared resource cannot be read locked and write locked at the same time.
* A shared resource may be locked for reading by multiple `lock::ReadLock` instances at a time.
* When a `ReadLock` / `WriteLock` goes out of scope, it releases its lock.
* When a `WriteLock` is moved, it transfers its lock to the target instance.
* A shared resource may not be moved, if there are locks remaining.

## Example #1: Single locks.
This example demonstrates the use of `lock::ThreadSafe` objects, as well as `lock::ReadLock` and `lock::WriteLock` for direct single locks.

```c++
ThreadSafe<int> resource(42);

void my_thread()
{
	// acquire a read lock on resource.
	lock::ReadLock<int> r_resource(resource);
	// access the stored value via *
	std::cout << *r_resource << std::endl;
}
void second_thread()
{
	// acquire a write lock on resource.
	lock::WriteLock<int> w_resource(resource);
	// access the stored value via *
	*w_resource = 1337;
}
```

## Example #2: Multi locks.
This example shows how to use multi locks to prevent dead locks.

```c++
lock::ThreadSafe<int> res_a(1337), res_b(42), res_c(0xdeadbeef);

// writes resources a and b; reads resource c
void thread1()
{
	lock::WriteLock<int> lock_a, lock_b;
	lock::ReadLock<int> lock_c;
	// write lock a and b, read lock c.
	lock::multi_lock(lock::pair(lock_a, res_a), lock::pair(lock_b, res_b), lock::pair(lock_c, res_c));

	(*lock_a) = (*lock_b) + (*lock_c);
	(*lock_b) = (*lock_a) + (*lock_c);
}

// writes resources b and c
void thread2()
{
	lock::WriteLock<int> lock_b, lock_c;
	lock::multi_write_lock(lock::pair(lock_b, res_b), lock::pair(lock_c, res_c));

	int temp = (*lock_b);
	(*lock_b) ^= (*lock_c);
	(*lock_c) = temp;
}

// reads all resources.
void thread3()
{
	lock::ReadLock<int> lock_a, lock_b, lock_c;
	lock::multi_read_lock(lock::pair(lock_a, res_a), lock::pair(lock_b, res_b), lock::pair(lock_c, res_c));
	
	std::cout << __FUNCTION__": a: " << *lock_a << ", b: " << *lock_b << ", c: " << *lock_c << "\n";
}
```

# Example #3: Range locks.
This example shows how to use range locks to lock a dynamic amount of objects.

```c++
std::vector<lock::ThreadSafe<int>> resources;

void thread(
	std::size_t n)
{
	// allocate locks.
	std::vector<lock::ReadLock<int>> reads(n);
	std::vector<lock::WriteLock<int>> writes(resources.size()-n);

	// reserve space for lock / resource pairings.
	std::vector<lock::ReadLockPair<int>> read_pairs;
	read_pairs.reserve(n);
	std::vector<lock::WriteLockPair<int> write_pairs;
	write_pairs.reserve(resources.size()-n);


	// set up the lock / resource pairs.
	for(std::size_t i = 0; i < n; i++)
		read_pairs.emplace_back(reads[i], resources[i]);
	for(std::size_t i = n; i < resources.size(); i++)
		write_pairs.emplace_back(writes[i], resources[i]);

	// acquire locks.
	lock::range_lock(
		lock::range(read_pairs.begin(), read_pairs.end()),
		lock::range(write_pairs.begin(), write_pairs.end()));

	int accum = 0;
	for(auto &read : reads)
		accum += *read;

	for(auto &write : writes)
		*writes += accum;
}

void main()
{
	std::size_t const n = 1000;
	resources.reserve(n);
	for(std::size_t i = 0; i < n; i++)
		resources.emplace_back(i);

	std::thread a(thread, n / 2), b(thread, n / 3);
	a.join(), b.join();
}
```
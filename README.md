# Lock
Lock is a Locking library for locking shared resources.
## Features
* Scoped read / write locks
* Ability to lock multiple resources in one call, preventing only partially locking the resources and dead locks. If you have to lock multiple resources, lock them using only one call! For read locks only, use ```lock::multi_read_lock```. For write locks only, use ```lock::multi_write_lock```. For mixed type locks, use ```lock::multi_lock```.

## Rules
* A shared resource may be locked for writing by only one WriteLock at a time.
* A shared resource cannot be read locked and write locked at the same time.
* A shared resource may be locked for reading by multiple ReadLocks at a time.
* When a ReadLock / WriteLock goes out of scope, it releases its lock.
* When a WriteLock is moved, it transfers its lock to the target instance.
* A shared resource may not be moved, if there are locks remaining.

## Example #1: Single locks.
```c++
#include "Lock/Lock.hpp"


#include <iostream>
#include <string>
#include <thread>


std::string unsafe_a("A quick brown fox jumps over the lazy dog."), unsafe_b("Franz jagt in seinem komplett verwahrlosten Taxi quer durch Bayern.");
lock::ThreadSafe<std::string> thread_safe("initial value");

void writer_thread1()
{
	lock::WriteLock<std::string> lock(thread_safe);
	for(unsigned i = 0; i<lock->size(); i++)
		(*lock)[i]++;

	std::cout << __FUNCTION__": " << *lock << "\n";
}
void writer_thread2()
{
	lock::WriteLock<std::string> lock(thread_safe);
	*lock = unsafe_a;
	*lock += unsafe_b;
	
	std::cout << __FUNCTION__": " << *lock << "\n";
}
void writer_thread3()
{
	// different way of acquiring locks
	lock::WriteLock<std::string> lock = thread_safe.writeLock();
	*lock += unsafe_a;

	std::cout << __FUNCTION__": " << *lock << "\n";
}
void reader_thread1()
{
	lock::ReadLock<std::string> lock(thread_safe);
	std::cout << __FUNCTION__": thread_safe[4] == '" << (*lock)[4] << "'\n";
}
void reader_thread2()
{
	lock::ReadLock<std::string> lock(thread_safe);
	std::cout << __FUNCTION__": thread_safe == '" << *lock << "'\n";
}

int main()
{
	std::thread w1(writer_thread1), w2(writer_thread2), w3(writer_thread3), r1(reader_thread1), r2(reader_thread2);
	w1.join(); w2.join(); w3.join(); r1.join(); r2.join();

	std::cin.ignore();
	return 0;
}
```

possible output:
```
writer_thread1: jojujbm!wbmvf
reader_thread1: thread_safe[4] == 'j'
reader_thread2: thread_safe == 'jojujbm!wbmvf'
writer_thread3: jojujbm!wbmvfA quick brown fox jumps over the lazy dog.
writer_thread2: A quick brown fox jumps over the lazy dog.Franz jagt in seinem komplett verwahrlosten Taxi quer durch Bayern.
```

## Example #2: Multi locks.
```c++
#include "Lock/Lock.hpp"


#include <iostream>
#include <string>
#include <thread>


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
	std::cout << __FUNCTION__": a: " << *lock_a << ", b: " << *lock_b << ", c: " << *lock_c << "\n";
}

// writes resources b and c
void thread2()
{
	lock::WriteLock<int> lock_b, lock_c;
	lock::multi_write_lock(lock::pair(lock_b, res_b), lock::pair(lock_c, res_c));

	int temp = (*lock_b);
	(*lock_b) ^= (*lock_c);
	(*lock_c) = temp;
	std::cout << __FUNCTION__": b: " << *lock_b << ", c: " << *lock_c << "\n";
}

// reads all resources.
void thread3()
{
	lock::ReadLock<int> lock_a, lock_b, lock_c;
	lock::multi_read_lock(lock::pair(lock_a, res_a), lock::pair(lock_b, res_b), lock::pair(lock_c, res_c));
	
	std::cout << __FUNCTION__": a: " << *lock_a << ", b: " << *lock_b << ", c: " << *lock_c << "\n";
}


int main()
{
	while(true)
	{
		std::thread t1(thread1), t2(thread2), t3(thread3);
		t1.join(); t2.join(); t3.join();

		std::cin.ignore();
	}
	return 0;
}
```
Possible output:
```
thread1: a: -559038695, b: -1118077432, c: -559038737
thread3: a: -559038695, b: -1118077432, c: -559038737
thread2: b: 1677115623, c: -1118077432
```

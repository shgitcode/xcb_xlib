#pragma once
#include "Global.h"

template<typename T>
class MutexDataPair {

public:
	class Lock {
	private:
		std::unique_lock<std::mutex> m_lock;
		T *m_data;
	public:
		inline Lock(MutexDataPair* parent) : m_lock(parent->m_mutex), m_data(&parent->m_data) {}
		inline T* operator->() { return m_data; }
		inline T* get() { return m_data; }
		inline std::unique_lock<std::mutex>& lock() { return m_lock; }
	};

private:
	std::mutex m_mutex;
	T m_data;

public:
	inline T* data() { return &m_data; } // useful in some rare cases (e.g. callback functions)

};

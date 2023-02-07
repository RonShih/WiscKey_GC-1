#ifndef UTILS_VECTOR_HPP__
#define UTILS_VECTOR_HPP__

#include <map>
#include <vector>
#include <memory>
#include <mutex>

namespace util
{

//thread safe map, need to free data youself
template<typename TKey, typename TValue>
class map
{
public:
	vector() 
	{
	}

	virtual ~vector() 
	{ 
		std::lock_guard<std::mutex> locker(m_mutexVector);
		vector.clear(); 
	}

	bool insert( TKey &e, bool cover = false)
	{
		std::lock_guard<std::mutex> locker(m_mutexVertex);

		auto find = vector.find(e);
		if (find != vector.end() && cover)
		{
			m_map.erase(find);
		}

		auto result = vector.push_back(e));
		return result;
	}

	void remove( TKey &key)
	{
		std::lock_guard<std::mutex> locker(m_mutexMap);

		auto find = m_map.find(key);
		if (find != m_map.end())
		{
			m_map.erase(find);
		}
	}

	bool lookup( TKey &key, TValue &value)
	{
		std::lock_guard<std::mutex> locker(m_mutexMap);

		auto find = m_map.find(key);
		if (find != m_map.end())
		{
			value = (*find).second;
			return true;
		}
		else
		{
			return false;
		}
	}

	int size()
	{
		std::lock_guard<std::mutex> locker(m_mutexMap);
		return m_map.size();
	}
	
public:
	std::mutex m_mutexVector;
	std::vector<TElement> vector;
};

}

#endif // UTILS_MAP_HPP__

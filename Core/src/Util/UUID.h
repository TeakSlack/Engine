#ifndef UUID_H
#define UUID_H

#include <random>
#include <unordered_set>

uint64_t GenerateUUID()
{
	static std::mt19937_64 rng(std::random_device{}());
	static std::unordered_set<uint64_t> usedUUIDs;
	static std::uniform_int_distribution<uint64_t> uniformDist;

	return uniformDist(rng);
}

#endif // UUID_H
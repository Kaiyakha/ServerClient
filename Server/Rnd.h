#pragma once
#include <random>

template <typename T>
class Rnd
{
private:
	std::mt19937 rng;
	std::uniform_int_distribution<T> uni;

public:
	Rnd(void) {};
	Rnd(const T distribution_start, const T distribution_end);
	Rnd(const T distribution_end) : Rnd(static_cast<T>(0), distribution_end) {}
	const T operator()(void);
};


template <typename T>
Rnd<T>::Rnd(const T distribution_start, const T distribution_end) {
	rng = std::mt19937(std::random_device{}());
	uni = std::uniform_int_distribution<T>(distribution_start, distribution_end);
}

template <typename T>
inline const T Rnd<T>::operator()(void) {
	return uni(rng);
}
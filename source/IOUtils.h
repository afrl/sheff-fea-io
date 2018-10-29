#pragma once

#include <fstream>
#include <vector>

namespace fea_io
{

	class IOUtils
	{
	public:
		/// Writes a fundamental type to a binary file a the current stream position
		template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type* = nullptr>
		static void binary_write(std::ofstream& out, const T& val);

		/// Writes a vector of fundamental types to a binary file a the current stream position
		template<typename T, int Size, typename std::enable_if<std::is_fundamental<T>::value, T>::type* = nullptr>
		static void binary_write(std::ofstream& out, const std::array<T, Size>& val);

		/// Writes a vector of fundamental types to a binary file a the current stream position
		template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type* = nullptr>
		static void binary_write(std::ofstream& out, const std::vector<T>& val);

		/// Writes an array of a fundamental type to a binary file a the current stream position
		template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type* = nullptr>
		static void binary_write(std::ofstream& out, T* arr, int size);
	};

	template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type*>
	inline void IOUtils::binary_write(std::ofstream & out, const T & val)
	{
		out.write(reinterpret_cast<const char*>(&val), sizeof(T));
	}

	template<typename T, int Size, typename std::enable_if<std::is_fundamental<T>::value, T>::type*>
	inline void IOUtils::binary_write(std::ofstream & out, const std::array<T, Size>& a)
	{
		out.write(reinterpret_cast<const char*>(&a[0]), static_cast<std::streamsize>(a.size() * sizeof(T)));
	}

	template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type*>
	inline void IOUtils::binary_write(std::ofstream & out, const std::vector<T>& v)
	{
		out.write(reinterpret_cast<const char*>(&v[0]), static_cast<std::streamsize>(v.size() * sizeof(T)));
	}

	template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type*>
	inline void IOUtils::binary_write(std::ofstream & out, T * arr, int size)
	{
		out.write(reinterpret_cast<const char*>(&arr[0]), static_cast<std::streamsize>(size * sizeof(T)));
	}

}
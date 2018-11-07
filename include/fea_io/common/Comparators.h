#pragma once

#include <vector>

namespace fea_io
{
    template<typename T0, typename T1>
	bool compare_vec(const std::vector<T0>& v0, const std::vector<T1>& v1)
	{
		if (v0.size() != v1.size()) return false;
		for (auto i = 0; i < v0.size(); i++)
			if (!(v0[i] == v1[i])) return false;
		return true;
	}

	template<typename T0, typename T1>
	bool compare_vec_vec(const std::vector<T0>& v0, const std::vector<T1>& v1)
	{
		if (v0.size() != v1.size()) return false;
		for (auto i = 0; i < v0.size(); i++)
			if (!compare_vec(v0[i], v1[i])) return false;
		return true;
	}
}
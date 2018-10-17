#pragma once

#include <stdint.h>

#include <nlohmann/json.hpp>


namespace fea_io
{
	using json = nlohmann::json;

	typedef int16_t GlobalIndex;
	typedef int16_t LocalIndex;
	typedef double CoordType;
};
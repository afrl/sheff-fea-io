#pragma once

#include "MeshStructs.h"
#include "IOUtils.h"
#include "Exceptions.h"

namespace fea_io
{
	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	class MeshReader
	{
	public:
		typedef MeshMetaInfo<GlobalIndex, LocalIndex, CoordType> MetaInfo;

		MeshReader(const std::string file_path);

	private:
		const MetaInfo meta_info;
	};
}
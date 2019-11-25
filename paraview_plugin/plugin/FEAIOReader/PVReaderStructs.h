#pragma once

namespace fea_io {
	class DataReader;
	struct ArrayInfo;
}

enum class ArrayToVTKCategory
{
	Point,
	PointAveraged,
	Cell,
	CellAveraged
};

struct ArrayReaderPair
{
	std::unique_ptr<fea_io::ArrayInfo> arrayInfo;
	/// Indicates hos this array should be converted to a VTK array
	ArrayToVTKCategory cateogry;
	fea_io::DataReader* readerPtr;
};
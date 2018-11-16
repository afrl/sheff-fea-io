#pragma once

#include "fea_io/data/DataStructs.h"

class fea_io::DataReader;

enum class ArrayToVTKCategory
{
	Point,
	PointAveraged,
	Cell,
	CellAveraged
};

struct ArrayReaderPair
{
	fea_io::ArrayInfo arrayInfo;
	/// Indicates hos this array should be converted to a VTK array
	ArrayToVTKCategory cateogry;
	fea_io::DataReader* readerPtr;
};
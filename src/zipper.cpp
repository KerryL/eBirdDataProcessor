// File:  zipper.cpp
// Date:  3/8/2018
// Auth:  K. Loux
// Desc:  Interface to .zip file compression/decompression library.

// Local headers
#include "zipper.h"

// libzip headers
#include <zip.h>

// Standard C++ headers
#include <cassert>

Zipper::~Zipper()
{
	if (archive)
		CloseArchive();
}

bool Zipper::OpenArchive(const String& fileName)
{
	assert(!archive && "Must only open one file at a time per object");
	archive = zip_open(UString::ToNarrowString(fileName).c_str(), ZIP_RDONLY, nullptr);
	return archive != nullptr;
}

void Zipper::CloseArchive()
{
	assert(archive && "Archive not yet open");
	zip_discard(archive);
}

String Zipper::GetErrorString() const
{
	return UString::ToStringType(zip_strerror(archive));
}

std::vector<String> Zipper::ListContents() const
{
	assert(archive && "Archive not yet open");
	const zip_int64_t numFiles(zip_get_num_entries(archive, 0));
	std::vector<String> fileList;

	zip_int64_t i;
	for (i = 0; i < numFiles; ++i)
	{
		struct zip_stat s;
		zip_stat_init(&s);
		if (zip_stat_index(archive, i, 0, &s) != 0)
			return std::vector<String>();

		if (s.valid & ZIP_STAT_NAME)
			fileList.push_back(UString::ToStringType(s.name));
	}

	return fileList;
}

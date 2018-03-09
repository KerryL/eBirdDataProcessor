// File:  zipper.cpp
// Date:  3/8/2018
// Auth:  K. Loux
// Desc:  Interface to .zip file compression/decompression library.

// Local headers
#include "zipper.h"

// Standard C++ headers
#include <cassert>
#include <limits>

Zipper::~Zipper()
{
	if (archive)
		CloseArchive();
}

bool Zipper::OpenArchiveFile(const String& fileName)
{
	assert(!archive && "Must only open one file at a time per object");
	archive = zip_open(UString::ToNarrowString(fileName).c_str(), ZIP_RDONLY, nullptr);
	return archive != nullptr;
}

bool Zipper::OpenArchiveBytes(const std::string& bytes)
{
	assert(!archive && "Must only open one file at a time per object");
	zip_source_t* buffer(zip_source_buffer_create(static_cast<const void*>(bytes.c_str()), bytes.length(), 0, nullptr));
	if (!buffer)
		return false;

	archive = zip_open_from_source(buffer, ZIP_RDONLY, nullptr);
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

bool Zipper::ExtractFile(const String& fileName, std::string& bytes)
{
	struct zip_stat s;
	zip_stat_init(&s);
	if (zip_stat(archive, UString::ToNarrowString(fileName).c_str(), 0, &s) != 0)
		return false;

	if (s.valid & ZIP_STAT_SIZE)
	{
		assert(s.size < std::numeric_limits<unsigned int>::max());
		bytes.resize(static_cast<unsigned int>(s.size));
	}
	else
		bytes.clear();

	zip_file_t* file(zip_fopen(archive, UString::ToNarrowString(fileName).c_str(), 0));
	if (!file)
		return false;

	return ReadAndCloseFile(file, bytes);
}

bool Zipper::ExtractFile(const zip_int64_t& index, std::string& bytes)
{
	struct zip_stat s;
	zip_stat_init(&s);
	if (zip_stat_index(archive, index, 0, &s) != 0)
		return false;

	if (s.valid & ZIP_STAT_SIZE)
	{
		assert(s.size < std::numeric_limits<unsigned int>::max());
		bytes.resize(static_cast<unsigned int>(s.size));
	}
	else
		bytes.clear();

	zip_file_t* file(zip_fopen_index(archive, index, 0));
	if (!file)
		return false;

	return ReadAndCloseFile(file, bytes);
}

bool Zipper::ReadAndCloseFile(zip_file_t* file, std::string& bytes) const
{
	std::vector<char> byteVector(bytes.size());
	if (byteVector.size() == 0)// Unknown file size
	{
		const unsigned int chunkSize(100);
		byteVector.resize(chunkSize);
		zip_int64_t bytesRead;
		do
		{
			bytesRead = zip_fread(file, static_cast<void*>(byteVector.data()), byteVector.size());
			assert(bytesRead < std::numeric_limits<unsigned int>::max());
			bytes.append(static_cast<const char*>(&byteVector.front()), static_cast<unsigned int>(bytesRead));
		} while (bytesRead == chunkSize);
	}
	else
	{
		if (zip_fread(file, static_cast<void*>(byteVector.data()), byteVector.size()) != bytes.size())
		{
			zip_fclose(file);
			return false;
		}
	}

	bytes.assign(&byteVector.front(), byteVector.size());
	return zip_fclose(file) == 0;
}

// File:  zipper.h
// Date:  3/8/2018
// Auth:  K. Loux
// Desc:  Interface to .zip file compression/decompression library.

#ifndef ZIPPER_H_

// Local headers
#include "utilities/uString.h"

// libzip headers
#include <zip.h>

// Standard C++ headers
#include <vector>

class Zipper
{
public:
	~Zipper();

	bool CreateArchiveFile(const UString::String& fileName);
	//bool CreateArchiveBytes(const std::string& bytes);// Currently not working, so removed from interface

	bool OpenArchiveFile(const UString::String& fileName);
	bool OpenArchiveBytes(const std::string& bytes);

	bool CloseArchive();
	bool ArchiveIsOpen() const { return archive != nullptr; }

	std::vector<UString::String> ListContents() const;

	bool ExtractFile(const UString::String& fileName, std::string& bytes);
	bool ExtractFile(const zip_int64_t& index, std::string& bytes);

	bool AddFile(const UString::String& fileNameInArchive, std::string& bytes);

	UString::String GetErrorString() const;

private:
	zip_t* archive = nullptr;
	bool writeChanges = false;

	bool ReadAndCloseFile(zip_file_t* file, std::string& bytes) const;

	UString::String errorString;

	bool OpenArchiveFile(const UString::String& fileName, const int& flags);
	bool OpenArchiveBytes(const std::string& bytes, const int& flags);
};

#endif// ZIPPER_H_

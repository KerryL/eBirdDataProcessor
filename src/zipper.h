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

	bool CreateArchiveFile(const String& fileName);
	bool CreateArchiveBytes(const std::string& bytes);

	bool OpenArchiveFile(const String& fileName);
	bool OpenArchiveBytes(const std::string& bytes);

	void CloseArchive();
	bool ArchiveIsOpen() const { return archive != nullptr; }

	std::vector<String> ListContents() const;

	bool ExtractFile(const String& fileName, std::string& bytes);
	bool ExtractFile(const zip_int64_t& index, std::string& bytes);

	bool AddFile(const String& fileNameInArchive, std::string& bytes);

	String GetErrorString() const;

private:
	zip_t* archive = nullptr;

	bool ReadAndCloseFile(zip_file_t* file, std::string& bytes) const;

	String errorString;

	bool OpenArchiveFile(const String& fileName, const int& flags);
	bool OpenArchiveBytes(const std::string& bytes, const int& flags);
};

#endif// ZIPPER_H_

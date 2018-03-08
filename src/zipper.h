// File:  zipper.h
// Date:  3/8/2018
// Auth:  K. Loux
// Desc:  Interface to .zip file compression/decompression library.

#ifndef ZIPPER_H_

// Local headers
#include "utilities/uString.h"

// Standard C++ headers
#include <vector>

// libzip forward declarations
struct zip;

class Zipper
{
public:
	~Zipper();

	bool OpenArchive(const String& fileName);
	void CloseArchive();

	std::vector<String> ListContents() const;

	String GetErrorString() const;

private:
	zip* archive = nullptr;

	String errorString;
};

#endif// ZIPPER_H_

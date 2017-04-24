// File:  eBirdDataProcessorApp.h
// Date:  4/20/2017
// Auth:  K. Loux
// Desc:  Application class for eBird Data Processor.

#ifndef EBIRD_DATA_PROCESSOR_APP_H_

// Standard C++ headers
#include <vector>
#include <string>

class CommandLineOption
{
public:
	CommandLineOption(const std::string& longForm, const std::string& shortForm,
		std::string& target, const std::string& defaultValue);
	CommandLineOption(const std::string& longForm, const std::string& shortForm,
		int& target, const int& defaultValue);
	CommandLineOption(const std::string& longForm, const std::string& shortForm,
		unsigned int& target, const unsigned int& defaultValue);
	CommandLineOption(const std::string& longForm, const std::string& shortForm,
		double& target, const double& defaultValue);
	CommandLineOption(const std::string& longForm, const std::string& shortForm,
		bool& target, const bool& defaultValue);

	bool Matches(const std::string& argument) const;
	bool Read(const std::string& argument, bool& consumeNext) const;
	bool IsTarget(const void* const check) const { return check == target; }

	std::string GetLongFormArgument() const { return longForm; }

private:
	const std::string longForm;
	const std::string shortForm;
	void* target;

	enum class Type
	{
		String,
		SignedInteger,
		UnsignedInteger,
		Double,
		Boolean
	};

	const Type type;
};

class EBirdDataProcessorApp
{
public:
	int Run(int argc, char *argv[]);

private:
	static const std::vector<CommandLineOption> availableOptions;
	static std::string GetLongFormArgument(const void* const target);

	bool ParseArguments(const std::vector<std::string>& arguments);
	bool ValidateOptions() const;

	struct Options
	{
		std::string dataFileName;

		std::string countryFilter;
		std::string stateFilter;
		std::string countyFilter;
		std::string locationFilter;

		unsigned int listType;// 0 - life, 1 - year, 2 - month, 3 - day

		bool totalsOnly;
	};

	static Options specifiedOptions;
};

#endif// EBIRD_DATA_PROCESSOR_APP_H_
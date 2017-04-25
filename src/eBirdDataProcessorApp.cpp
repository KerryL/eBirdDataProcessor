// File:  eBirdDataProcessorApp.cpp
// Date:  4/20/2017
// Auth:  K. Loux
// Desc:  Application class for eBird Data Processor.

// Local headers
#include "eBirdDataProcessorApp.h"
#include "eBirdDataProcessor.h"

// Standard C++ headers
#include <sstream>
#include <iostream>
#include <cassert>
#include <fstream>

int main(int argc, char *argv[])
{
	EBirdDataProcessorApp app;
	return app.Run(argc, argv);
}

CommandLineOption::CommandLineOption(const std::string& longForm,
	const std::string& shortForm, std::string& target, const std::string& defaultValue)
	: longForm(longForm), shortForm(shortForm), target(static_cast<void*>(&target)), type(Type::String)
{
	assert(shortForm.empty() == longForm.empty());
	target = defaultValue;
}

CommandLineOption::CommandLineOption(const std::string& longForm,
	const std::string& shortForm, int& target, const int& defaultValue)
	: longForm(longForm), shortForm(shortForm), target(static_cast<void*>(&target)), type(Type::SignedInteger)
{
	assert(shortForm.empty() == longForm.empty());
	target = defaultValue;
}

CommandLineOption::CommandLineOption(const std::string& longForm,
	const std::string& shortForm, unsigned int& target, const unsigned int& defaultValue)
	: longForm(longForm), shortForm(shortForm), target(static_cast<void*>(&target)), type(Type::UnsignedInteger)
{
	assert(shortForm.empty() == longForm.empty());
	target = defaultValue;
}

CommandLineOption::CommandLineOption(const std::string& longForm,
	const std::string& shortForm, double& target, const double& defaultValue)
	: longForm(longForm), shortForm(shortForm), target(static_cast<void*>(&target)), type(Type::Double)
{
	assert(shortForm.empty() == longForm.empty());
	target = defaultValue;
}
CommandLineOption::CommandLineOption(const std::string& longForm,
	const std::string& shortForm, bool& target, const bool& defaultValue)
	: longForm(longForm), shortForm(shortForm), target(static_cast<void*>(&target)), type(Type::Boolean)
{
	assert(shortForm.empty() == longForm.empty());
	target = defaultValue;
}

bool CommandLineOption::Matches(const std::string& argument) const
{
	if (longForm.empty() && shortForm.empty())
		return true;

	if (argument.length() < 2 || argument[0] != '-')
		return false;

	if (argument[1] == '-')
		return longForm.compare(argument.substr(2)) == 0;

	return shortForm.compare(argument.substr(1)) == 0;
}

bool CommandLineOption::Read(const std::string& argument, bool& consumeNext) const
{
	if (type == Type::Boolean)
	{
		consumeNext = false;
		*static_cast<bool*>(target) = true;
		return true;
	}

	if (!consumeNext && !longForm.empty() && !shortForm.empty())
	{
		consumeNext = true;
		return true;
	}

	consumeNext = false;

	std::istringstream ss(argument);
	switch (type)
	{
	case Type::String:
		*static_cast<std::string*>(target) = argument;
		return true;

	case Type::SignedInteger:
		return (ss >> *static_cast<int*>(target)).good();

	case Type::UnsignedInteger:
		return (ss >> *static_cast<unsigned int*>(target)).good();

	case Type::Double:
		return (ss >> *static_cast<double*>(target)).good();

	case Type::Boolean:// Should never get here
	default:
		break;
	}

	return false;
}

EBirdDataProcessorApp::Options EBirdDataProcessorApp::specifiedOptions;
const std::vector<CommandLineOption> EBirdDataProcessorApp::availableOptions({
	CommandLineOption("", "", specifiedOptions.dataFileName, ""),
	CommandLineOption("output", "o", specifiedOptions.dataFileName, ""),
	CommandLineOption("country", "c", specifiedOptions.countryFilter, ""),
	CommandLineOption("state", "s", specifiedOptions.stateFilter, ""),
	CommandLineOption("county", "C", specifiedOptions.countyFilter, ""),
	CommandLineOption("location", "l", specifiedOptions.locationFilter, ""),
	CommandLineOption("listType", "t", specifiedOptions.listType, 0),
	CommandLineOption("totalsOnly", "T", specifiedOptions.totalsOnly, false),
	CommandLineOption("allSightings", "A", specifiedOptions.allSightings, false),
	CommandLineOption("year", "y", specifiedOptions.listType, 0),
	CommandLineOption("month", "m", specifiedOptions.listType, 0),
	CommandLineOption("week", "w", specifiedOptions.listType, 0),
	CommandLineOption("day", "d", specifiedOptions.listType, 0),
	CommandLineOption("sortBy", "1", specifiedOptions.primarySort, 0),
	CommandLineOption("thenBy", "2", specifiedOptions.secondarySort, 0)
});

std::string EBirdDataProcessorApp::GetLongFormArgument(const void* const target)
{
	for (const auto& option : availableOptions)
	{
		if (option.IsTarget(target))
			return option.GetLongFormArgument();
	}

	assert(false);
	return std::string();
}

int EBirdDataProcessorApp::Run(int argc, char *argv[])
{
	const std::vector<std::string> arguments(argv, argv + argc);
	if (!ParseArguments(arguments))
		return 1;

	if (!ValidateOptions())
		return 1;

	EBirdDataProcessor processor;
	if (!processor.Parse(specifiedOptions.dataFileName))
		return 1;

	// Remove entires that don't fall withing the specified locations
	if (!specifiedOptions.locationFilter.empty())
		processor.FilterLocation(specifiedOptions.locationFilter, specifiedOptions.countyFilter,
			specifiedOptions.stateFilter, specifiedOptions.countryFilter);
	else if (!specifiedOptions.countyFilter.empty())
		processor.FilterCounty(specifiedOptions.countyFilter, specifiedOptions.stateFilter,
			specifiedOptions.countryFilter);
	else if (!specifiedOptions.stateFilter.empty())
		processor.FilterState(specifiedOptions.stateFilter, specifiedOptions.countryFilter);
	else if (!specifiedOptions.countryFilter.empty())
		processor.FilterCountry(specifiedOptions.countryFilter);

	// Remove entries that don't fall within the specified dates
	if (specifiedOptions.yearFilter > 0)
		processor.FilterYear(specifiedOptions.yearFilter);

	if (specifiedOptions.monthFilter > 0)
		processor.FilterMonth(specifiedOptions.monthFilter);

	if (specifiedOptions.weekFilter > 0)
		processor.FilterWeek(specifiedOptions.weekFilter);

	if (specifiedOptions.dayFilter > 0)
		processor.FilterDay(specifiedOptions.dayFilter);


	// TODO:  Totals option? all sightings?

	processor.SortData(static_cast<EBirdDataProcessor::SortBy>(specifiedOptions.primarySort),
		static_cast<EBirdDataProcessor::SortBy>(specifiedOptions.secondarySort));

	const std::string list(processor.GenerateList());
	std::cout << list << std::endl;

	if (!specifiedOptions.outputFileName.empty())
	{
		std::ofstream outFile(specifiedOptions.outputFileName.c_str());
		if (!outFile.is_open() || !outFile.good())
		{
			std::cerr << "Failed to open '" << specifiedOptions.outputFileName << "' for output\n";
			return 1;
		}

		outFile << list << std::endl;
	}

	return 0;
}
bool EBirdDataProcessorApp::ParseArguments(const std::vector<std::string>& arguments)
{
	for (const auto& option : availableOptions)
	{
		bool consumeNext(false);
		for (const auto& argument : arguments)
		{
			if (consumeNext || option.Matches(argument))
			{
				if (!option.Read(argument, consumeNext))
					return false;
			}
		}
	}

	return true;
}

bool EBirdDataProcessorApp::ValidateOptions() const
{
	bool configurationOK(true);
	if (specifiedOptions.dataFileName.empty())
	{
		std::cerr << "Must specify argument '" << GetLongFormArgument(
			static_cast<void*>(&specifiedOptions.dataFileName)) << '\n';
		configurationOK = false;
	}

	if (!specifiedOptions.countryFilter.empty() &&
		specifiedOptions.countryFilter.length() != 2)
	{
		std::cerr << "Country must be specified using 2-digit abbreviation\n";
		configurationOK = false;
	}

	if (!specifiedOptions.stateFilter.empty() &&
		specifiedOptions.stateFilter.length() != 2)
	{
		std::cerr << "State/providence must be specified using 2-digit abbreviation\n";
		configurationOK = false;
	}

	if (specifiedOptions.dayFilter > 31)
	{
		std::cerr << "Day argument must be in the range 0 - 31\n";
		configurationOK = false;
	}

	if (specifiedOptions.monthFilter > 12)
	{
		std::cerr << "Month argument must be in the range 0 - 12\n";
		configurationOK = false;
	}

	if (specifiedOptions.weekFilter > 53)
	{
		std::cerr << "Week argument must be in the range 0 - 53\n";
		configurationOK = false;
	}

	if (specifiedOptions.listType > 4)
	{
		std::cerr << "List type argument must be in the range 0 - 4\n";
		configurationOK = false;
	}

	return configurationOK;
}

/* amSynth
 * (c) 2001-2005 Nick Dowell
 */

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

#include "TuningMap.h"

using namespace std;

TuningMap::TuningMap		()
{
	defaultScale();
	defaultKeyMap();
}

void
TuningMap::defaultScale		()
{
	scaleDesc = "12-per-octave equal temperament (default)";
	scale.clear();
	for (int i = 1; i <= 12; ++i)
		scale.push_back(pow(2., i/12.));
	updateBasePitch();
}

void
TuningMap::defaultKeyMap	()
{
	zeroNote = 0;
	refNote = 69;
	refPitch = 440.;
	mapRepeatInc = 1;
	mapping.clear();
	mapping.push_back(0);
	updateBasePitch();
}

void
TuningMap::updateBasePitch	()
{
	if (mapping.empty())
		return; // must be just initializing
	basePitch = 1.;
	basePitch = refPitch / noteToPitch(refNote);
	// Clever, huh?
}

double
TuningMap::noteToPitch		(int note) const
{
	assert(note >= 0 && note < 128);
	assert(!mapping.empty());

	int mapSize = mapping.size();

	int nRepeats = (note - zeroNote) / mapSize;
	int mapIndex = (note - zeroNote) % mapSize;
	if (mapIndex < 0)
	{
		nRepeats -= 1;
		mapIndex += mapSize;
	}

	if (mapping[mapIndex] < 0)
		return -1.; // unmapped note

	int scaleDegree = nRepeats * mapRepeatInc + mapping[mapIndex];

	int scaleSize = scale.size();

	int nOctaves = scaleDegree / scaleSize;
	int scaleIndex = scaleDegree % scaleSize;
	if (scaleIndex < 0)
	{
		nOctaves -= 1;
		scaleIndex += scaleSize;
	}

	if (scaleIndex == 0)
		return basePitch * pow(scale[scaleSize - 1], nOctaves);
	else
		return basePitch * pow(scale[scaleSize - 1], nOctaves) * scale[scaleIndex - 1];
}

// Convert a single line of a Scala scale file to a frequency relative to 1/1.
double
parseScalaLine(const string & line)
{
	istringstream iss(line);
	if (line.find('.') == string::npos)
	{ // treat as ratio
		long n, d;
		char slash;
		iss >> n >> slash >> d;
		if (iss.fail() || slash != '/' || n <= 0 || d <= 0)
		{
			return -1;
		}
		return (double) n / d;
	}
	else
	{ // treat as cents
		double cents;
		iss >> cents;
		if (iss.fail())
		{
			return -1;
		}
		return pow(2., cents/1200.);
	}
}

int
TuningMap::loadScale		(const string & filename)
{
	ifstream file(filename.c_str());
	string line;

	string newScaleDesc;
	bool gotDesc = false;
	int scaleSize = -1;
	vector<double> newScale;

	while (file.good())
	{
		getline(file, line);
		unsigned i = 0;
		while (i < line.size() && isspace(line[i])) ++i;
		if (line[i] == '!') continue;	// skip comment lines

		// skip all-whitespace lines after description
		if (i == line.size() && gotDesc) continue;

		if (!gotDesc)
		{
			newScaleDesc = line;
			gotDesc = true;
		}
		else if (scaleSize < 0)
		{
			istringstream iss(line);
			iss >> scaleSize;
			if (scaleSize < 0)
			{
				return -1;
			}
		}
		else
			newScale.push_back(parseScalaLine(line));
	}

	if (!gotDesc || (int) newScale.size() != scaleSize)
		return -1;

	scaleDesc = newScaleDesc;
	scale = newScale;
	updateBasePitch();
	return 0;
}

int
TuningMap::loadKeyMap		(const string & filename)
{
	ifstream file(filename.c_str());
	string line;

	int mapSize = -1;
	int firstNote = -1;
	int lastNote = -1;
	int newZeroNote = -1;
	int newRefNote = -1;
	double newRefPitch = -1.;
	int newMapRepeatInc = -1;
	vector<int> newMapping;

	while (file.good())
	{
		getline(file, line);
		unsigned i = 0;
		while (i < line.size() && isspace(line[i])) ++i;
		if (i == line.size()) continue;	// skip all-whitespace lines
		if (line[i] == '!') continue;	// skip comment lines
		istringstream iss(line);
		if (mapSize < 0)
		{
			iss >> mapSize;
			if (iss.fail() || mapSize < 0)
				return -1;
		}
		else if (firstNote < 0)
		{
			iss >> firstNote;
			if (iss.fail() || firstNote < 0 || firstNote >= 128)
				return -1;
		}
		else if (lastNote < 0)
		{
			iss >> lastNote;
			if (iss.fail() || lastNote < 0 || lastNote >= 128)
				return -1;
		}
		else if (newZeroNote < 0)
		{
			iss >> newZeroNote;
			if (iss.fail() || newZeroNote < 0 || newZeroNote >= 128)
				return -1;
		}
		else if (newRefNote < 0)
		{
			iss >> newRefNote;
			if (iss.fail() || newRefNote < 0 || newRefNote >= 128)
				return -1;
		}
		else if (newRefPitch <= 0)
		{
			iss >> newRefPitch;
			if (iss.fail() || newRefPitch <= 0)
				return -1;
		}
		else if (newMapRepeatInc < 0)
		{
			iss >> newMapRepeatInc;
			if (iss.fail() || newMapRepeatInc < 0)
				return -1;
		}
		else if (tolower(line[i]) == 'x')
			newMapping.push_back(-1); // unmapped key
		else
		{
			int mapEntry;
			iss >> mapEntry;
			if (iss.fail() || mapEntry < 0)
				return -1;
			newMapping.push_back(mapEntry);
		}
	}

	if (newMapRepeatInc < 0) return -1; // didn't get far enough

	if (mapSize == 0)
	{ // special case for "automatic" linear mapping
		if (!newMapping.empty())
			return -1;
		zeroNote = newZeroNote;
		refNote = newRefNote;
		refPitch = newRefPitch;
		mapRepeatInc = 1;
		mapping.clear();
		mapping.push_back(0);
		updateBasePitch();
		return 0;
	}

// some of the kbm files included with Scala have extra x's at the end for no good reason
//	if ((int) newMapping.size() > mapSize)
//		return -1;

	newMapping.resize(mapSize, -1);

	// Check to make sure reference pitch is actually mapped
	int refIndex = (newRefNote - newZeroNote) % mapSize;
	if (refIndex < 0)
		refIndex += mapSize;
	if (newMapping[refIndex] < 0)
		return -1;

	zeroNote = newZeroNote;
	refNote = newRefNote;
	refPitch = newRefPitch;

	if (newMapRepeatInc == 0)
		mapRepeatInc = mapSize;
	else
		mapRepeatInc = newMapRepeatInc;

	mapping = newMapping;
	updateBasePitch();
	return 0;
}

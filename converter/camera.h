#pragma once
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>


/**
* Container for the camera path data
*/
struct Camera {
	std::vector<glm::vec3> positions;
	std::vector<bool> original;
	int origSize;
};

/**
* Lookup table for a single segment of the camera path
*/
struct ArcLengthTable {

	/** Pre-calculated positions on the path */
	std::vector<glm::vec3> samples;

	/** Pre-calculated tangents of the samples */
	std::vector<glm::vec3> samplesTan;

	std::vector<float> normalizedDistance;
	float length = 0.0f;
	std::vector<float> segmentLengths;
	std::vector<float> segmentStartDistances;

	float totalLength = 0.0f;
	int samplesPerSegment = 100;
};

/**
* Construct lookup tables for camera animation based on lines
* 
* @param camera Camera data
* @return Vector of lookup tables for separate path segments
*/
std::vector<ArcLengthTable> GetPathLines(Camera& camera);

/**
* Construct lookup tables for camera animation based on Catmull-rom spline
*
* @param camera Camera data
* @return Vector of lookup tables for separate path segments
*/
std::vector<ArcLengthTable> GetPathCurves(Camera& camera);

/**
* Getcurrent camera position based on time elapsed and lookup tables
* 
* @param table Lookup table for the path segment
* @param t Time from 0 to 1
* @param normalizedDistance List of distances according to lookup table positions
* @return Interpolated camera position
*/
float GetCatmullRomT(const ArcLengthTable& table, int segment, float time, float startSpeed, float endSpeed);

/**
* Exports camera points as a sequence of 3D points into .txt format
*
* @param points sequence of points to export
* @param filename Name of the export file
*/
void exportCamera(const std::string& pName, std::vector<ArcLengthTable>& pathLUT, std::vector<float> jointSpeeds);

glm::vec3 EvaluateHermiteArcLength(
	const ArcLengthTable& table,
	float time,
	float startSpeed,
	float endSpeed);



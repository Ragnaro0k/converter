#pragma once
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <vector>


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
glm::vec3 EvaluateArcLength(const std::vector<glm::vec3>& table, float t, std::vector<float> normalizedDistance);



#include "camera.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include <iostream>
#include <fstream>

/**
* Calculates tangent to a point on the path
* 
* @param points Sequence of control points on the path
* @param segment Index of the first first control point
* @param t Normalized time
* @return Non-normalized tangent to the point on the curve
*/
glm::vec3 CatmullRomTangent(const std::vector<glm::vec3>& points, int segment, float t) {
    int n = static_cast<int>(points.size());

    t = std::clamp(t, 0.0f, 1.0f);

    const glm::vec3& p1 = points[segment + 1];
    const glm::vec3& p2 = points[segment + 2];

    const glm::vec3& p0 =
        points[segment];

    const glm::vec3& p3 =
        points[segment + 3];

    float t2 = t * t;

    return 0.5f * ((-p0 + p2) + 2.0f * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t + 3.0f * (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t2);
}

/**
* Calculates a position on the path according to Catmull-rom curve
* 
* @param points Sequence of control points on the path
* @param segment Index of the first first control point
* @param t Normalized time
* @return Position on the curve
*/
glm::vec3 CatmullRomSegment(const std::vector<glm::vec3>& points, int segment, float t){
	int n = static_cast<int>(points.size());

	t = std::clamp(t, 0.0f, 1.0f);

	const glm::vec3& p1 = points[segment + 1];
	const glm::vec3& p2 = points[segment + 2];

	const glm::vec3& p0 =
		points[segment];

	const glm::vec3& p3 =
		points[segment + 3];
    
	float t2 = t * t;
	float t3 = t2 * t;

	return 0.5f * (2.0f * p1 + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

glm::vec3 LineSegment(const std::vector<glm::vec3>& points, int segment, float t) {
    int n = static_cast<int>(points.size());

    t = std::clamp(t, 0.0f, 1.0f);

    const glm::vec3& p1 = points[segment+1];
    const glm::vec3& p2 = points[segment + 2];

    return (1 - t) * p1 + t * p2;
}

/**
* Construct a lookup table for camera path
* 
* @param points Control points on the path
* @param curves Switch between Catmull Rom curve and straightline path variant
* @param samplesPerSegment Number of samples per segment
* @return Lookup table for the path segment
*/
ArcLengthTable BuildArcLengthTable(const std::vector<glm::vec3>& points, bool curves, int samplesPerSegment = 100){
    ArcLengthTable table;

    int numSegments = static_cast<int>(points.size()) - 3;

    // Sample the spline
    for (int i = 0; i < numSegments; ++i)
    {
        for (int j = 0; j < samplesPerSegment; ++j)
        {
            float t =
                static_cast<float>(j) /
                static_cast<float>(samplesPerSegment);

            glm::vec3 p = curves ? CatmullRomSegment(points, i, t) : LineSegment(points, i, t);

            table.samples.push_back(p);

            if (curves) {
                glm::vec3 tan = glm::normalize(CatmullRomTangent(points, i, t));
                table.samplesTan.push_back(tan);
            }
        }
    }

    // Add final endpoint
    table.samples.push_back(points[points.size()-2]);
    int lastSegment = numSegments - 1;
    glm::vec3 finalTangent = glm::normalize((points[points.size()-1]-points[points.size()-3])/2.0f);

    table.samplesTan.push_back(finalTangent);
    /*for (int i = 0; i < table.samplesTan.size(); ++i) {
        std::cout << "Sample " << i << " is " << table.samplesTan[i].x << " " << table.samplesTan[i].y << " " << table.samplesTan[i].z << " " << std::endl;
    }
    std::cout << std::endl;*/
    // First sample is at distance 0
    table.normalizedDistance.push_back(0.0f);

    float totalLength = 0.0f;

    // Calculate cumulative distances
    for (size_t i = 1; i < table.samples.size(); ++i)
    {
        float distance =
            glm::length(
                table.samples[i] -
                table.samples[i - 1]
            );

        totalLength += distance;

        table.normalizedDistance.push_back(totalLength);
    }

    // Normalize to [0, 1]
    if (totalLength > 0.0f)
    {
        for (float& d : table.normalizedDistance)
        {
            d /= totalLength;
        }
    }

    return table;
}

glm::vec3 EvaluateArcLength(const std::vector<glm::vec3>& table, float t, std::vector<float> normalizedDistance){
    //std::cout << "Evaluation called" << std::endl;
    t = std::clamp(t, 0.0f, 1.0f);

    const auto& distances = normalizedDistance;
    const auto& samples = table;

    // Find first distance >= t
    auto it = std::lower_bound(
        distances.begin(),
        distances.end(),
        t
    );

    if (it == distances.begin()) {
        //std::cout << "returning begin" << std::endl;
        return samples.front();
    }

    if (it == distances.end()) {
        //std::cout << "returning back" << std::endl;
        return samples.back();
    }

    size_t i = std::distance(distances.begin(), it);

    float t0 = distances[i - 1];
    float t1 = distances[i];

    float alpha = (t - t0) / (t1 - t0);

    return glm::mix(
        samples[i - 1],
        samples[i],
        alpha
    );
}

std::vector<ArcLengthTable> GetPathCurves(Camera &camera) {
    std::vector<ArcLengthTable> ret;
    if (!camera.original[0] || !camera.original[camera.original.size() - 1]) return ret;
    int offset = 0;
    for (int i = 0; i < camera.origSize; i++) {
        //process each proper segment
        std::vector<glm::vec3> points;
        std::cout << "Segment " << i << " starting at index " << i + offset << std::endl;
        points.push_back(i == 0 ? camera.positions[i + offset] : camera.positions[i - 1 + offset]);
        points.push_back(camera.positions[i + offset]);
        while (!camera.original[i + offset + 1]) {
            points.push_back(camera.positions[i + offset + 1]);
            offset++;
        }
        points.push_back(camera.positions[i + offset + 1]);
        points.push_back(i == camera.origSize - 2 ? camera.positions[i + offset + 1] : camera.positions[i + offset + 2]);
        std::cout << "Segment " << i << " ending at index " << i + offset + 2 << std::endl;

        //here I should have the complete segment points
        std::cout << "Segment " << i << " contains " << points.size() << " points" << std::endl;
        ret.push_back(BuildArcLengthTable(points, true));
        std::cout << std::endl;
    }
    return ret;
}

std::vector<ArcLengthTable> GetPathLines(Camera &camera) {
    std::vector<ArcLengthTable> ret;
	if (!camera.original[0] || !camera.original[camera.original.size()-1]) return ret;
	int offset = 0;
	for (int i = 0; i < camera.origSize; i++) {
		//process each proper segment
		std::vector<glm::vec3> points;
        points.push_back(i == 0 ? camera.positions[i + offset] : camera.positions[i - 1 + offset]);
		points.push_back(camera.positions[i + offset]);
		while (!camera.original[i + offset + 1]) {
			points.push_back(camera.positions[i + offset+1]);
			offset++;
		}
        points.push_back(camera.positions[i + offset + 1]);
        points.push_back(i == camera.origSize - 2 ? camera.positions[i + offset + 1] : camera.positions[i + offset + 2]);

		//here I should have the complete segment points
        ret.push_back(BuildArcLengthTable(points, false));
	}
    return ret;
}



void exportCamera(const std::string& pName, std::vector<ArcLengthTable>& pathLUT) {
    std::ofstream file(pName);
    int seconds = static_cast<int>(pathLUT.size());
    uint32_t version = 1;
    uint32_t frameCount = seconds * 60;
    //std::cout << seconds << " seconds" << std::endl;
    glm::vec3 camPos;
    glm::vec3 camDir;
    glm::vec3 camUp = glm::vec3(0.0f, 1.0f, 0.0f);
    float viewT;

    for (int i = 0; i < seconds-1; ++i) {
        //std::cout << "Evaluating segment " << i << std::endl;
        for (size_t t = 0; t < 60; ++t) {
            //std::cout << "Frame " << t << " out of 60" << std::endl;
            camPos = EvaluateArcLength(pathLUT[i].samples, static_cast<float>(t)/60.0f, pathLUT[i].normalizedDistance);
            viewT = static_cast<float>(t)/60.0f + 0.5f;
            while (viewT >= 1.0f) {
                viewT -= 1.0f;
            }
            if (viewT < static_cast<float>(t) / 60.0f) {
                //std::cout << "Case 1" << std::endl;
                camDir = i + 1 >= pathLUT.size() - 1 ? pathLUT[pathLUT.size()-1].samples[pathLUT[pathLUT.size() - 1].samples.size()-1] : EvaluateArcLength(pathLUT[i + 1].samples, viewT, pathLUT[i + 1].normalizedDistance);
            }
            else {
                //std::cout << "Case 2" << std::endl;
                camDir = EvaluateArcLength(pathLUT[i].samples, viewT, pathLUT[i].normalizedDistance);
            }
            camDir = glm::normalize(camDir);
            glm::vec3 pivot = camPos + camDir;
            glm::vec3 relativePosition = -camDir;

            file
                // pivot
                << pivot.x << " " << pivot.y << " " << pivot.z << " "

                // up
                << camUp.x << " " << camUp.y << " " << camUp.z << " "

                // position relative to pivot
                << relativePosition.x << " "
                << relativePosition.y << " "
                << relativePosition.z << " ";

            // Five identity quaternions
            for (int i = 0; i < 5; ++i)
                file << "0 0 0 1 ";

            // yFov (radians), zNear, zFar
            file << "1.0367 0.1 10000.0\n";
        }
    }
    if (!file)
        throw std::runtime_error("Failed to export camera");
    file.close();
    return;
}

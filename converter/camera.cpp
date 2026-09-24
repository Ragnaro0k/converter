#include "camera.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include <iostream>

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
        std::cout << "returning begin" << std::endl;
        return samples.front();
    }

    if (it == distances.end()) {
        std::cout << "returning back" << std::endl;
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

std::vector<ArcLengthTable> GetCameraPath(Camera &camera, bool useCurves) {
	return useCurves ? GetPathCurves(camera) : GetPathLines(camera);
}
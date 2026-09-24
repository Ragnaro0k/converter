#pragma once
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <vector>



struct Camera {
	std::vector<glm::vec3> positions;
	std::vector<bool> original;
	int origSize;
};

struct ArcLengthTable {
	std::vector<glm::vec3> samples;
	std::vector<glm::vec3> samplesTan;
	std::vector<float> normalizedDistance;
};

glm::vec3 CatmullRomSegment(const std::vector<glm::vec3>& points, int segment, float t);

std::vector<ArcLengthTable> GetCameraPath(Camera& camera, bool useCurves);

glm::vec3 EvaluateArcLength(const std::vector<glm::vec3>& table, float t, std::vector<float> normalizedDistance);

glm::vec3 CatmullRomTangent(const std::vector<glm::vec3>& points, int segment, float t);


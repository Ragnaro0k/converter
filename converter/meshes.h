#pragma once
#include <glad/glad.h>
#include <vector>
#include <string>
#include <glm/gtc/type_ptr.hpp>
#include <limits>


struct Mesh {
	std::vector<glm::vec3> vertices;
	std::vector<uint32_t> triangles;
};


struct RenderMesh {
	std::vector<glm::vec3> vertices;
	std::vector<uint32_t> triangles;
	std::vector<glm::vec3> normals;
	GLuint VAO;
	GLuint VBO;
	GLuint EBO;
};

struct BoundingBox {
	float Xplus = 0;
	float Xminus = 0;
	float Yplus = 0;
	float Yminus = 0;
	float Zplus = 0;
	float Zminus = 0;
	float lim_Xplus = std::numeric_limits<float>::min();
	float lim_Xminus = std::numeric_limits<float>::max();
	float lim_Yplus = std::numeric_limits<float>::min();
	float lim_Yminus = std::numeric_limits<float>::max();
	float lim_Zplus = std::numeric_limits<float>::min();
	float lim_Zminus = std::numeric_limits<float>::max();
};

void rotateMesh(Mesh& mesh);

std::vector<Mesh> importMesh(const std::vector<std::string>& paths, std::vector<RenderMesh>& renderMesh, int randSampl, BoundingBox& box);

void exportMeshes(const std::vector<Mesh>& meshes, const std::string& filename, BoundingBox& box);

void exportReduced(const std::vector<Mesh>& meshes, const std::string& filename, BoundingBox& box);

#include "meshes.h"
#include <glad/glad.h>
#include <iostream>
#include <fstream>
#include <GLFW/glfw3.h>
#include <string>
#include <pybind11/embed.h>
#include <pybind11/numpy.h>
#include <cstdlib>
#include <vector>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>

const size_t MAX_TRIANGLES = 1'000'000;

namespace py = pybind11;

struct RawMesh {
	std::vector<glm::vec3> vertices;
	std::vector<uint32_t> indices;
	std::vector<uint32_t> counts;
	glm::mat4 transform;
};


std::vector<RawMesh> loadFromPython(const std::vector<std::string>& paths) {
	std::cout << "Starting python loader..." << std::endl;
	std::vector<RawMesh> result;
	pybind11::list pyPaths;
	for (const auto& p : paths) {
		pyPaths.append(p);
	}
	std::cout << "Calling python..." << std::endl;
	py::module loader = py::module::import("usd_loader");
	py::object pyMeshes = loader.attr("load_meshes")(pyPaths);
	std::cout << "Python returned " << std::endl;
	int meshCount = 0;
	int verts = 0;
	for (auto pyMesh : pyMeshes) {
		meshCount++;
		RawMesh mesh;

		py::array_t<float> points = pyMesh["points"].cast<py::array_t<float>>();
		auto pbuf = points.request();

		float* pdata = static_cast<float*>(pbuf.ptr);
		size_t vertexCount = pbuf.shape[0];

		mesh.vertices.resize(vertexCount);
		for (size_t i = 0; i < vertexCount; i++) {
			mesh.vertices[i] = glm::vec3(
				pdata[i * 3 + 0],
				pdata[i * 3 + 1],
				pdata[i * 3 + 2]
			);
		}

		py::array_t<uint32_t> indices = pyMesh["indices"].cast<py::array_t<uint32_t>>();
		auto ibuf = indices.request();

		uint32_t* idata = static_cast<uint32_t*>(ibuf.ptr);
		mesh.indices.assign(idata, idata + ibuf.shape[0]);

		py::array_t<uint32_t> counts = pyMesh["counts"].cast<py::array_t<uint32_t>>();
		auto cbuf = counts.request();

		uint32_t* cdata = static_cast<uint32_t*>(cbuf.ptr);
		mesh.counts.assign(cdata, cdata + cbuf.shape[0]);

		py::array_t<float> matrix = pyMesh["matrix"].cast<py::array_t<float>>();
		auto mbuf = matrix.request();

		float* mdata = static_cast<float*>(mbuf.ptr);

		mesh.transform = glm::make_mat4(mdata);

		verts += mesh.vertices.size();
		result.push_back(std::move(mesh));
	}
	std::cout << meshCount << " of meshes imported" << std::endl;
	std::cout << verts << " of vertices imported" << std::endl;
	return result;
}

void applyTransform(RawMesh& mesh) {
	for (auto& v : mesh.vertices) {
		glm::vec4 tmp = mesh.transform * glm::vec4(v, 1.0f);
		v = glm::vec3(tmp);
	}
}

std::vector<uint32_t> triangulate(const RawMesh& mesh, int randSampl) {
	std::vector<uint32_t> triangles;
	std::vector<uint32_t> ret;

	uint32_t offset = 0;

	for (uint32_t count : mesh.counts) {
		if (count == 3) {
			triangles.insert(triangles.end(),
				mesh.indices.begin() + offset,
				mesh.indices.begin() + offset + 3);
		}
		else {
			std::vector<uint32_t> vertices;
			for (int i = 0; i < count; i++) {
				vertices.push_back(mesh.indices[offset + i]);
			}
			for (int i = 1; i < count - 1; i++) {
				triangles.insert(triangles.end(), {
					vertices[0], vertices[i], vertices[i + 1]
					});
			}
		}

		offset += count;
	}
	if (randSampl > 1) {
		for (size_t i = 0; i + 2 < triangles.size(); i += 3) {
			if (rand() % randSampl == 0) {
				ret.insert(ret.end(), {
					triangles[i],
					triangles[i + 1],
					triangles[i + 2]
					});
			}
		}
	}
	else {
		ret = triangles;
	}
	return ret;
}

void rotateMesh(Mesh& mesh) {
	glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(-1.0f, 0.0f, 0.0f));
	glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.025f));
	for (auto& v : mesh.vertices) {
		glm::vec4 transform = rotation * glm::vec4(v, 1.0f);
		transform = scale * transform;
		v = glm::vec3(transform);
	}
		return;
}

std::vector<Mesh> importMesh(const std::vector<std::string>& paths, std::vector<RenderMesh>& renderMesh, int randSampl, BoundingBox& box) {
	std::vector<RawMesh> rawMeshes;
	try {
		rawMeshes = loadFromPython(paths);
	}
	catch (py::error_already_set& e) {
		std::cerr << "Python error:\n" << e.what() << std::endl;
	}
	std::cout << "Imported " << rawMeshes.size() << " meshes" << std::endl;
	std::vector<Mesh> meshes;
	std::cout << "Rotating meshes..." << std::endl;
	for(auto& m : rawMeshes){
		applyTransform(m);

		auto triangles = triangulate(m, randSampl);
		Mesh tmp;
		tmp.vertices = m.vertices;
		tmp.triangles = triangles;
		rotateMesh(tmp);
		meshes.push_back(tmp);
	}
	std::cout << "Preparing RenderMesh..." << std::endl;
	uint32_t vertexOffset = 0;
	RenderMesh tmp;
	for (auto& mesh : meshes) {
		tmp.vertices.insert(tmp.vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
		for (uint32_t idx : mesh.triangles) {
			tmp.triangles.push_back(idx + vertexOffset);
		}
		vertexOffset += static_cast<uint32_t>(mesh.vertices.size());
		if (vertexOffset > MAX_TRIANGLES) {
			std::cout << "Buffer limit reached, creating new buffer..." << std::endl;
			renderMesh.push_back(tmp);
			tmp.vertices.clear();
			tmp.triangles.clear();
			vertexOffset = 0;
		}
	}
	if (!tmp.vertices.empty()) {
		renderMesh.push_back(tmp);
	}

	for (auto& mesh : meshes) {
		for (auto& vert : mesh.vertices) {
			if (vert.x > box.lim_Xplus) box.lim_Xplus = vert.x;
			if (vert.x < box.lim_Xminus) box.lim_Xminus = vert.x;
			if (vert.y > box.lim_Yplus) box.lim_Yplus = vert.y;
			if (vert.y < box.lim_Yminus) box.lim_Yminus = vert.y;
			if (vert.z > box.lim_Zplus) box.lim_Zplus = vert.z;
			if (vert.z < box.lim_Zminus) box.lim_Zminus = vert.z;
		}
	}
	box.Xplus = box.lim_Xplus;
	box.Xminus = box.lim_Xminus;
	box.Yplus = box.lim_Yplus;
	box.Yminus = box.lim_Yminus;
	box.Zplus = box.lim_Zplus;
	box.Zminus = box.lim_Zminus;

	std::cout << "Initializing Buffers..." << std::endl;
	if (!renderMesh.empty()) {
		std::cout << "Clearing buffers..." << std::endl;
		for (auto& mesh : renderMesh) {
			glDeleteVertexArrays(1, &mesh.VAO);
			glDeleteBuffers(1, &mesh.VBO);
			glDeleteBuffers(1, &mesh.EBO);
		}

	}
	std::cout << "Generating bufers..." << std::endl;
	for (auto& mesh : renderMesh) {
		glGenVertexArrays(1, &mesh.VAO);
		glGenBuffers(1, &mesh.VBO);
		glGenBuffers(1, &mesh.EBO);

		glBindVertexArray(mesh.VAO);


		glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
		glBufferData(GL_ARRAY_BUFFER,
			mesh.vertices.size() * sizeof(glm::vec3),
			mesh.vertices.data(),
			GL_STATIC_DRAW);


		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
			mesh.triangles.size() * sizeof(uint32_t),
			mesh.triangles.data(),
			GL_STATIC_DRAW);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glEnableVertexAttribArray(0);

		glBindVertexArray(0);
	}

	std::cout << "Returning mesh..." << std::endl;
	return meshes;
}

bool isInBounds(glm::vec3 vertex, BoundingBox& box) {
	if (vertex.x > box.Xplus) return false;
	if (vertex.x < box.Xminus) return false;
	if (vertex.y > box.Yplus) return false;
	if (vertex.y < box.Yminus) return false;
	if (vertex.z > box.Zplus) return false;
	if (vertex.z < box.Zminus) return false;
	return true;
}

void exportReduced(const std::vector<Mesh>& meshes, const std::string& filename, BoundingBox& box) {
	std::ofstream file(filename);
	if (!file.is_open()) {
		throw std::runtime_error("Cannot open file: " + filename);
	}
	int count = 1;
	int vertexOffset = 1;

	for (auto& mesh : meshes) {
		Mesh reducedmesh;
		std::vector<bool> vertexUsed(mesh.vertices.size(), false);

		for (uint32_t i = 0; i < mesh.triangles.size(); i += 3) {
			if (isInBounds(mesh.vertices[mesh.triangles[i]], box) || isInBounds(mesh.vertices[mesh.triangles[i + 1]], box) || isInBounds(mesh.vertices[mesh.triangles[i + 2]], box)) {
				reducedmesh.triangles.push_back(mesh.triangles[i]);
				reducedmesh.triangles.push_back(mesh.triangles[i+1]);
				reducedmesh.triangles.push_back(mesh.triangles[i+2]);
				vertexUsed[mesh.triangles[i]] = true;
				vertexUsed[mesh.triangles[i+1]] = true;
				vertexUsed[mesh.triangles[i+2]] = true;
			}
		}
		uint32_t vertexCount = 0;
		std::unordered_map<uint32_t, uint32_t> reducedOffset;
		for (uint32_t i = 0; i < mesh.vertices.size(); i++) {
			if (vertexUsed[i]) {
				reducedmesh.vertices.push_back(mesh.vertices[i]);
				reducedOffset[i] = vertexCount;
				vertexCount++;
			}
		}
		for (uint32_t i = 0; i < reducedmesh.triangles.size(); i++) {
			reducedmesh.triangles[i] = reducedOffset[reducedmesh.triangles[i]];
		}

		file << "o _" << count << "Shape\n";

		for (const auto& v : reducedmesh.vertices) {
			file << "v " << v.x << " " << v.y << " " << v.z << "\n";
		}

		for (int i = 0; i < reducedmesh.triangles.size(); i += 3) {
			file << "f " << reducedmesh.triangles[i] + vertexOffset << " " << reducedmesh.triangles[i + 1] + vertexOffset << " " << reducedmesh.triangles[i + 2] + vertexOffset << "\n";
		}

		vertexOffset += reducedmesh.vertices.size();
		count++;
	}
	std::cout << "Exported reduced!" << std::endl;
}

void exportMeshes(const std::vector<Mesh>& meshes, const std::string& filename, BoundingBox& box) {
	std::ofstream file(filename);
	if (!file.is_open()) {
		throw std::runtime_error("Cannot open file: " + filename);
	}
	int count = 1;
	int vertexOffset = 1;
	for (auto mesh : meshes) {

		file << "o _" << count << "Shape\n";

		for (const auto& v : mesh.vertices) {
			file << "v " << v.x << " " << v.y << " " << v.z << "\n";
		}


		for (int i = 0; i < mesh.triangles.size(); i+=3) {
			if (isInBounds(mesh.vertices[mesh.triangles[i]], box) || isInBounds(mesh.vertices[mesh.triangles[i+1]], box) || isInBounds(mesh.vertices[mesh.triangles[i+2]], box)) {
				file << "f " << mesh.triangles[i] + vertexOffset << " " << mesh.triangles[i + 1] + vertexOffset << " " << mesh.triangles[i + 2] + vertexOffset << "\n";
			}
		}

		vertexOffset += mesh.vertices.size();
		count++;
	}


	file.close();
	std::cout << "Exported!" << std::endl;
}


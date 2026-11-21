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
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <cstdint>
#include <glm/glm.hpp>
#include "glm/ext.hpp"
#include <unordered_map>

/** Maximum number of triangles per buffer */
const size_t MAX_TRIANGLES = 1'000'000;

namespace py = pybind11;

/**
* Auxiliary container for mesh processing
*/
struct RawMesh {
	std::vector<glm::vec3> vertices;
	std::vector<uint32_t> indices;
	std::vector<uint32_t> counts;
	std::string name;
	float mpu;
	glm::mat4 transform;
};
/**
* @return Total sum of cells covered by triangles 
*/
int getAABBvector(std::vector<int>& segmentsAABB, const std::vector<Mesh>& meshes, BoundingBox& box,
					float xStep, float yStep, float zStep, const int segmentsRow) {

	int cellsCovered = 0;
	for (auto& m : meshes) {
		for (int i = 0; i < m.triangles.size(); i += 3) {
			glm::vec3 v0 = m.vertices[m.triangles[i]];
			glm::vec3 v1 = m.vertices[m.triangles[i+1]];
			glm::vec3 v2 = m.vertices[m.triangles[i+2]];

			glm::vec3 minP = glm::min(glm::min(v0, v1), v2);
			glm::vec3 maxP = glm::max(glm::max(v0, v1), v2);

			int minX = static_cast<int>((minP.x - box.Xminus) / xStep);
			int maxX = static_cast<int>((maxP.x - box.Xminus) / xStep);

			int minY = static_cast<int>((minP.y - box.Yminus) / yStep);
			int maxY = static_cast<int>((maxP.y - box.Yminus) / yStep);

			int minZ = static_cast<int>((minP.z - box.Zminus) / zStep);
			int maxZ = static_cast<int>((maxP.z - box.Zminus) / zStep);

			minX = std::clamp(minX, 0, segmentsRow - 1);
			maxX = std::clamp(maxX, 0, segmentsRow - 1);
			minY = std::clamp(minY, 0, segmentsRow - 1);
			maxY = std::clamp(maxY, 0, segmentsRow - 1);
			minZ = std::clamp(minZ, 0, segmentsRow - 1);
			maxZ = std::clamp(maxZ, 0, segmentsRow - 1);


			for (int z = minZ; z <= maxZ; z++) {
				for (int y = minY; y <= maxY; y++) {
					for (int x = minX; x <= maxX; x++) {
						cellsCovered++;
						segmentsAABB[y * segmentsRow + x + z * segmentsRow * segmentsRow]++;
					}
				}
			}
		}
	}
	return cellsCovered;
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

void exportStats(const std::vector<Mesh>& meshes, std::string fname, bool reduced, BoundingBox& box) {

	std::vector<Mesh> reducedMeshes;

	for (auto& mesh : meshes) {
		Mesh reducedmesh;
		std::vector<bool> vertexUsed(mesh.vertices.size(), false);

		for (uint32_t i = 0; i < mesh.triangles.size(); i += 3) {
			if (isInBounds(mesh.vertices[mesh.triangles[i]], box) || isInBounds(mesh.vertices[mesh.triangles[i + 1]], box) || isInBounds(mesh.vertices[mesh.triangles[i + 2]], box)) {
				reducedmesh.triangles.push_back(mesh.triangles[i]);
				reducedmesh.triangles.push_back(mesh.triangles[i + 1]);
				reducedmesh.triangles.push_back(mesh.triangles[i + 2]);
				vertexUsed[mesh.triangles[i]] = true;
				vertexUsed[mesh.triangles[i + 1]] = true;
				vertexUsed[mesh.triangles[i + 2]] = true;
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
		if (!reducedmesh.vertices.empty()) {
			reducedmesh.name = mesh.name;
			reducedMeshes.push_back(reducedmesh);
		}
	}

	std::cout << "Starting data export..." << std::endl;
	int segmentsRow = 128;
	int triangles = 0;
	int verts = 0;
	float sizeX = -1;
	float sizeY = -1;
	float sizeZ = -1;
	float volume = -1;
	float density = -1; // triangles divided by volume; triangles per cubic metre
	float avArea = 0; // average surface of a triangle - expand to contain more data?
	float distribution = 0;
	float occupancyRatio = -1;
	int objects = 0;
	double cv = -1;


	sizeX = box.Xplus - box.Xminus;
	sizeY = box.Yplus - box.Yminus;
	sizeZ = box.Zplus - box.Zminus;
	volume = sizeX * sizeY * sizeZ;

	float xStep = sizeX / segmentsRow;
	float yStep = sizeY / segmentsRow;
	float zStep = sizeZ / segmentsRow;

	std::vector<int> segments(segmentsRow * segmentsRow * segmentsRow, 0);
	std::vector<int> segmentsAABB(segmentsRow * segmentsRow * segmentsRow, 0);
	int AABBcoverage = getAABBvector(segmentsAABB, reducedMeshes, box, xStep, yStep, zStep, segmentsRow);


	std::cout << "Initiating for loop..." << std::endl;
	for (auto& m : reducedMeshes) {
		objects++;
		triangles += m.triangles.size() / 3;
		verts += m.vertices.size();

		for (int i = 0; i < m.triangles.size(); i += 3) {
			//std::cout << "vectors" << std::endl;
			glm::vec3 vec1 = glm::vec3(m.vertices[m.triangles[i + 1]] - m.vertices[m.triangles[i]]);
			glm::vec3 vec2 = glm::vec3(m.vertices[m.triangles[i + 2]] - m.vertices[m.triangles[i]]);
			float area = glm::length(glm::cross(vec1, vec2)) / 2;
			avArea += area;

			//calculate distribution data
			//std::cout << "distribution" << std::endl;
			glm::vec3 centroid = glm::vec3(m.vertices[m.triangles[i]] + m.vertices[m.triangles[i + 1]] + m.vertices[m.triangles[i + 2]]);
			centroid /= 3;
			int x = (centroid.x - box.Xminus) / xStep;
			int y = (centroid.y - box.Yminus) / yStep;
			int z = (centroid.z - box.Zminus) / zStep;
			x = std::clamp(x, 0, segmentsRow-1);
			y = std::clamp(y, 0, segmentsRow-1);
			z = std::clamp(z, 0, segmentsRow-1);

			//std::cout << "segments" << std::endl;
			segments[y * segmentsRow + x + z * segmentsRow * segmentsRow]++;


		}

	}
	std::cout << "Calculating avg area..." << std::endl;
	avArea = (avArea / static_cast<float>(triangles))*10000;
	density = static_cast<float>(triangles) / volume;

	//evaluate distribution
	int fullCells = 0;
	for (auto& cell : segments) {
		if (cell > 0) {
			fullCells++;
			float p = static_cast<float>(cell) / static_cast<float>(triangles);
			distribution -= p * glm::log2(p);
		}
	}

	//normalize
	if (fullCells > 0) distribution = distribution / glm::log2(static_cast<float>(fullCells));

	std::cout << "full cells: " << fullCells << std::endl;

	//occupancy ratio
	occupancyRatio = (static_cast<float>(fullCells) / static_cast<float>(segmentsRow * segmentsRow * segmentsRow)) * 100;
	double mean = static_cast<double>(triangles) /
		static_cast<double>(fullCells);
	double squaredSum = 0.0;
	std::cout << "Opening file..." << std::endl;
	//fname.replace(fname.end() - 4, fname.end(), "_stats.txt");
	std::ofstream file(fname);
	if (!file.is_open()) {
		throw std::runtime_error("Cannot open file: " + fname + "_stats.txt");
	}

	for (auto cell : segments)
	{
		if (cell > 0)
		{
			double diff = cell - mean;
			squaredSum += diff * diff;
		}
	}
	double variance = squaredSum / fullCells;
	double stddev = std::sqrt(variance);
	cv = stddev / mean;

	std::vector<int> occupancies;
	for (auto cell : segments) {
		if (cell > 0) occupancies.push_back(cell);
	}
	std::sort(occupancies.begin(), occupancies.end());
	double sum = 0.0;
	double weightedSum = 0.0;
	for (int i = 0; i < occupancies.size(); ++i) {
		double x = occupancies[i];
		sum += x;
		weightedSum += (i + 1) * x;
	}
	double n = static_cast<double>(occupancies.size());
	double gini = (2.0 * weightedSum) / (n * sum) - (n + 1.0) / n;

	std::sort(segments.begin(), segments.end());
	sum = 0.0;
	weightedSum = 0.0;
	for (int i = 0; i < segments.size(); ++i) {
		double x = segments[i];
		sum += x;
		weightedSum += (i + 1) * x;
	}
	double n2 = static_cast<double>(segments.size());
	double giniFull = (2.0 * weightedSum) / (n2 * sum) - (n2 + 1.0) / n2;

	int AABBfullCells = 0;
	float AABBdistribution = 0;
	for (auto& cell : segmentsAABB) {
		if (cell > 1) {
			AABBfullCells++;
			float p = static_cast<float>(cell) / static_cast<float>(AABBcoverage);
			AABBdistribution -= p * glm::log2(p);
		}
	}
	if (AABBfullCells > 1) AABBdistribution = AABBdistribution / glm::log2(static_cast<float>(AABBfullCells));
	else AABBdistribution = 0;

	double avgTriangleSegments = static_cast<double>(AABBcoverage) / static_cast<double>(triangles);
	float AABBoccupancyRatio = (static_cast<float>(AABBfullCells) / static_cast<float>(segmentsRow * segmentsRow * segmentsRow)) * 100;

	std::vector<int> AABBoccupancies;
	for (auto cell : segmentsAABB) {
		if (cell > 0) AABBoccupancies.push_back(cell);
	}
	std::sort(AABBoccupancies.begin(), AABBoccupancies.end());
	double AABBsum = 0.0;
	double AABBweightedSum = 0.0;
	for (int i = 0; i < AABBoccupancies.size(); ++i) {
		double x = AABBoccupancies[i];
		AABBsum += x;
		AABBweightedSum += (i + 1) * x;
	}
	double AABBn = static_cast<double>(AABBoccupancies.size());
	double AABBgini = (2.0 * AABBweightedSum) / (AABBn * AABBsum) - (AABBn + 1.0) / AABBn;

	std::sort(segmentsAABB.begin(), segmentsAABB.end());
	AABBsum = 0.0;
	AABBweightedSum = 0.0;
	for (int i = 0; i < segmentsAABB.size(); ++i) {
		double AABBx = segmentsAABB[i];
		AABBsum += AABBx;
		AABBweightedSum += (i + 1) * AABBx;
	}
	double AABBn2 = static_cast<double>(segmentsAABB.size());
	double AABBginiFull = (2.0 * AABBweightedSum) / (AABBn2 * AABBsum) - (AABBn2 + 1.0) / AABBn2;

	file << "Number of segments used to divide the scene: " << segmentsRow << "^3" << std::endl;
	file << "Number of meshes: " << objects << std::endl;
	file << "Number of triangles: " << triangles << std::endl;
	file << "Average number of triangles per object: " << static_cast<float>(triangles)/static_cast<float>(objects) << std::endl;
	file << "Number of vertices: " << verts << std::endl;
	file << "Volume in cubic metres: " << volume << " (" << sizeX << " * " << sizeY << " * " << sizeZ << ")" << std::endl;
	file << "Average surface area of a single triangle (square cm): " << avArea << std::endl;
	file << "Average triangle count per 1 cubic metre: " << density << std::endl;
	file << std::endl;
	file << "Data based on triangle centroids:" << std::endl;
	file << "Scene occupancy ratio (segments containing at least one triangle): " << occupancyRatio << "%" << std::endl;
	file << "Coefficient of variation: " << cv << std::endl;
	file << "Normalized triangle distribution (0 = high amount of clusters, 1 = even distribution): " << distribution << std::endl;
	file << "Gini coefficient of occupied cells (1 = all triangle centroids present in a single cell, 0 = triangles centroids are evenly distributed): " << gini << std::endl;
	file << "Gini coefficient of all cells (includes empty cells in the calculation): " << giniFull << std::endl;
	file << std::endl;
	file << "Data based on triangle AABB dimensions:" << std::endl;
	file << "Average triangle segments coverage (average number of segments a triangle covers): " << avgTriangleSegments << std::endl;
	file << "Triangle AABB occupancy ratio (segments containing at least one triangle): " << AABBoccupancyRatio << "%" << std::endl;
	file << "Normalized triangle distribution (0 = high amount of clusters, 1 = even distribution): " << AABBdistribution << std::endl;
	file << "Gini coefficient of occupied cells (1 = all triangle centroids present in a single cell, 0 = triangles centroids are evenly distributed): " << AABBgini << std::endl;
	file << "Gini coefficient of all cells (includes empty cells in the calculation): " << AABBginiFull << std::endl;
	
	file << std::endl;
	file << "Bounding box values:" << std::endl;
	file << "X size: " << (box.Xplus - box.Xminus)/ (box.lim_Xplus - box.lim_Xminus) << std::endl;
	file << "X offset: " << (box.Xminus - box.lim_Xminus)/ (box.lim_Xplus - box.lim_Xminus) << std::endl;
	file << "Y size: " << (box.Yplus - box.Yminus) / (box.lim_Yplus - box.lim_Yminus) << std::endl;
	file << "Y offset: " << (box.Yminus - box.lim_Yminus) / (box.lim_Yplus - box.lim_Yminus) << std::endl;
	file << "Z size: " << (box.Zplus - box.Zminus) / (box.lim_Zplus - box.lim_Zminus) << std::endl;
	file << "Z offset: " << (box.Zminus - box.lim_Zminus) / (box.lim_Zplus - box.lim_Zminus) << std::endl;

	file.close();
	std::cout << "Statistics exported to " << fname << std::endl;

}

/**
* Import meshes from .usd/.usda files
*/
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
	int brushCounter = 0;
	for (auto pyMesh : pyMeshes) {
		meshCount++;
		RawMesh mesh;
		mesh.name = pyMesh["name"].cast<std::string>();
		std::size_t found = mesh.name.find("sxmyjg cgyjc");
		if (/*found == std::string::npos && !(mesh.name.substr(0, 3) == "geo")*/true) {
			//mesh.mpu = pyMesh["mpu"].cast<float>();

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

			mesh.transform =glm::make_mat4(
					reinterpret_cast<float*>(mdata));
			/*if (mesh.name.substr(0, 3) == "geo") {
				std::cout << "Mesh " << mesh.name << "has matrix" << std::endl;
				std::cout << glm::to_string(mesh.transform) << std::endl;
				std::cout << std::endl;
			}*/

			verts += mesh.vertices.size();
			result.push_back(std::move(mesh));
		}

	}
	//std::cout << brushCounter << " of brushes was located" << std::endl;
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

/**
* Convert quads and polygons with more than 3 sides into triangles
*/
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

/**
* Mesh orientation fix
*/
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

std::vector<Mesh> importMesh(const std::vector<std::string>& paths, std::vector<RenderMesh>& renderMesh, int randSampl, BoundingBox& box, glm::vec3 *target) {
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
		tmp.name = m.name;
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
			//std::cout << "Buffer limit reached, creating new buffer..." << std::endl;
			renderMesh.push_back(tmp);
			tmp.vertices.clear();
			tmp.triangles.clear();
			vertexOffset = 0;
		}
	}
	if (!tmp.vertices.empty()) {
		renderMesh.push_back(tmp);
	}
	glm::vec3 camEye = glm::vec3(0.0f);
	for (auto& mesh : meshes) {
		for (auto& vert : mesh.vertices) {
			mesh.centre += vert;
			if (vert.x > box.lim_Xplus) box.lim_Xplus = vert.x;
			if (vert.x < box.lim_Xminus) box.lim_Xminus = vert.x;
			if (vert.y > box.lim_Yplus) box.lim_Yplus = vert.y;
			if (vert.y < box.lim_Yminus) box.lim_Yminus = vert.y;
			if (vert.z > box.lim_Zplus) box.lim_Zplus = vert.z;
			if (vert.z < box.lim_Zminus) box.lim_Zminus = vert.z;
		}
		mesh.centre /= static_cast<float>(mesh.vertices.size());
		camEye += mesh.centre;
	}
	*target = camEye / static_cast<float>(meshes.size());

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

std::vector<glm::vec3> GetCube(glm::vec3 point) {
	//point = glm::vec3(0.0f);
	std::vector<glm::vec3> ret(8);
	ret[0] = glm::vec3(point.x - 0.5f, point.y, point.z - 0.5f);
	ret[1] = glm::vec3(point.x + 0.5f, point.y, point.z - 0.5f);
	ret[2] = glm::vec3(point.x + 0.5f, point.y, point.z + 0.5f);
	ret[3] = glm::vec3(point.x - 0.5f, point.y, point.z + 0.5f);
	ret[4] = glm::vec3(point.x - 0.5f, point.y + 3.0f, point.z - 0.5f);
	ret[5] = glm::vec3(point.x + 0.5f, point.y + 3.0f, point.z - 0.5f);
	ret[6] = glm::vec3(point.x + 0.5f, point.y + 3.0f, point.z + 0.5f);
	ret[7] = glm::vec3(point.x - 0.5f, point.y + 3.0f, point.z + 0.5f);
	return ret;
}
/*
* Increase the amount of player paths to export creating new ones from existing paths, adding an offset to them
* @param players existing player paths
* @param nPaths desired maximal number of total paths (cannot be lower than number of original paths)
* @return original player paths expanded with new ones
**/
std::vector<Player> GetPathsCount(std::vector<Player> players, int nPaths) {
	int index = 0;
	int loop = 1;
	bool flag = true;
	int origSize = static_cast<int>(players.size());
	std::vector<std::unordered_map <int, bool>> usedIndices(origSize);
	while (players.size() < nPaths) {

		//if the index reached the end of the original vector, check if at least one new node was made; if not, break the cycle, otherwise increase loop and reset index
		if (index == origSize) {
			if (flag) { //no new node was found suitable for expansion
				std::cout << "No new suitable node was found for expansion, exiting..." << std::endl;
				break;
			}
			flag = true;
			index = 0;
			loop++;
			std::cout << "\nStarting loop " << loop << std::endl;

		}

		if (static_cast<int>(players[index].positions.size()) <= loop + 1) {
			std::cout << "Node " << players[index].id << " no longer suitable for expansion" << std::endl;
			index++;
			continue;
		}
		
		//expand the node
		int startIdx = 0;
		int divisor = static_cast<int>(players[index].positions.size()) > 10 ? 10 : 2;
		int shift = static_cast<int>(players[index].positions.size() / divisor);
		int overflow = (loop * shift) / static_cast<int>(players[index].positions.size());
		std::cout << "Size " << players[index].positions.size() << ", divisor " << divisor << ", shift " << shift << ", overflow " << overflow << std::endl;
		startIdx = ((loop * shift) + overflow) % static_cast<int>(players[index].positions.size());
		std::cout << "Original value is " << startIdx << std::endl;
		while (usedIndices[index].contains(startIdx) || startIdx == 0) {
			startIdx++;
			startIdx %= static_cast<int>(players[index].positions.size());
		}
		std::cout << "Final value is " << startIdx << std::endl;
		usedIndices[index][startIdx] = true;
		if (startIdx >= static_cast<int>(players[index].positions.size())) std::cout << "ERROR: Node " << players[index].id << " created index of " << startIdx << ", limit is " << players[index].positions.size() << std::endl;
		flag = false;
		Player newPath = players[index];
		newPath.pathStart = startIdx;
		std::cout << "Node " << players[index].id << " has been expanded: ID " << static_cast<int>(players.size()) << ", offset " << startIdx << std::endl;
		newPath.id = static_cast<int>(players.size());
		players.push_back(newPath);
		//move the start of the vector to the new index and put everything that was before it, behind it

		index++;

		
	}
	
	return players;
}

void exportReduced(const std::vector<Mesh>& meshes, const std::string& filename, BoundingBox& box, bool &cubes, std::vector<Player>& players, int nPaths) {
	std::vector<Mesh> reducedMeshes;
	std::vector<Player> newPlayers = GetPathsCount(players, nPaths);
	//std::vector<Player> newPlayers = players;
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
		if (!reducedmesh.vertices.empty()) {
			reducedmesh.name = mesh.name;
			reducedMeshes.push_back(reducedmesh);
		}
	}

	std::ofstream file(filename);
	if (!file.is_open()) {
		throw std::runtime_error("Cannot open file: " + filename);
	}
	int vertexOffset = 1;
	for(auto& mesh : reducedMeshes) {
		//std::cout << mesh.name << std::endl;
		file << "o " << mesh.name << "\n";

		for (const auto& v : mesh.vertices) {
			file << "v " << v.x << " " << v.y << " " << v.z << "\n";
		}

		for (int i = 0; i < mesh.triangles.size(); i += 3) {
			file << "f " << mesh.triangles[i] + vertexOffset << " " << mesh.triangles[i + 1] + vertexOffset << " " << mesh.triangles[i + 2] + vertexOffset << "\n";
		}

		vertexOffset += mesh.vertices.size();

	}
	if (cubes) {
		//need to create a cube at the starting position of every player
		for (auto& p : newPlayers) {
			file << "o player\_" << p.id << "\n";
			glm::vec3 start = p.positions[p.pathStart];
			for (auto& v : GetCube(start)) {
				file << "v " << v.x << " " << v.y << " " << v.z << "\n";
			}
			file << "f " << vertexOffset + 0 << " " << vertexOffset + 2 << " " << vertexOffset + 1 << "\n";
			file << "f " << vertexOffset + 0 << " " << vertexOffset + 3 << " " << vertexOffset + 2 << "\n";
			file << "f " << vertexOffset + 0 << " " << vertexOffset + 1 << " " << vertexOffset + 5 << "\n";
			file << "f " << vertexOffset + 0 << " " << vertexOffset + 5 << " " << vertexOffset + 4 << "\n";
			file << "f " << vertexOffset + 1 << " " << vertexOffset + 2 << " " << vertexOffset + 6 << "\n";
			file << "f " << vertexOffset + 0 << " " << vertexOffset + 6 << " " << vertexOffset + 5 << "\n";
			file << "f " << vertexOffset + 2 << " " << vertexOffset + 3 << " " << vertexOffset + 6 << "\n";
			file << "f " << vertexOffset + 3 << " " << vertexOffset + 7 << " " << vertexOffset + 6 << "\n";
			file << "f " << vertexOffset + 4 << " " << vertexOffset + 5 << " " << vertexOffset + 6 << "\n";
			file << "f " << vertexOffset + 4 << " " << vertexOffset + 6 << " " << vertexOffset + 7 << "\n";
			file << "f " << vertexOffset + 3 << " " << vertexOffset + 0 << " " << vertexOffset + 4 << "\n";
			file << "f " << vertexOffset + 4 << " " << vertexOffset + 7 << " " << vertexOffset + 3 << "\n";

			vertexOffset += 8;
		}
	}
	file.close();
	std::cout << "Exported reduced scene to " << filename << std::endl;
}

void exportMeshes(const std::vector<Mesh>& meshes, const std::string& filename, bool &cubes, std::vector<Player>& players, int nPaths) {
	std::vector<Player> newPlayers = GetPathsCount(players, nPaths);
	std::ofstream file(filename);
	if (!file.is_open()) {
		throw std::runtime_error("Cannot open file: " + filename);
	}
	int vertexOffset = 1;
	for (auto mesh : meshes) {

		file << "o " << mesh.name << "\n";

		for (const auto& v : mesh.vertices) {
			file << "v " << v.x << " " << v.y << " " << v.z << "\n";
		}


		for (int i = 0; i < mesh.triangles.size(); i+=3) {
			file << "f " << mesh.triangles[i] + vertexOffset << " " << mesh.triangles[i + 1] + vertexOffset << " " << mesh.triangles[i + 2] + vertexOffset << "\n";
		}

		vertexOffset += mesh.vertices.size();
	}

	if (cubes) {
		//need to create a cube at the starting position of every player
		for (auto& p : newPlayers) {
			file << "o player\_" << p.id << "\n";
			glm::vec3 start = p.positions[p.pathStart];
			for (auto& v : GetCube(start)) {
				file << "v " << v.x << " " << v.y << " " << v.z << "\n";
			}
			file << "f " << vertexOffset + 0 << " " << vertexOffset + 2 << " " << vertexOffset + 1 << "\n";
			file << "f " << vertexOffset + 0 << " " << vertexOffset + 3 << " " << vertexOffset + 2 << "\n";
			file << "f " << vertexOffset + 0 << " " << vertexOffset + 1 << " " << vertexOffset + 5 << "\n";
			file << "f " << vertexOffset + 0 << " " << vertexOffset + 5 << " " << vertexOffset + 4 << "\n";
			file << "f " << vertexOffset + 1 << " " << vertexOffset + 2 << " " << vertexOffset + 6 << "\n";
			file << "f " << vertexOffset + 0 << " " << vertexOffset + 6 << " " << vertexOffset + 5 << "\n";
			file << "f " << vertexOffset + 2 << " " << vertexOffset + 3 << " " << vertexOffset + 6 << "\n";
			file << "f " << vertexOffset + 3 << " " << vertexOffset + 7 << " " << vertexOffset + 6 << "\n";
			file << "f " << vertexOffset + 4 << " " << vertexOffset + 5 << " " << vertexOffset + 6 << "\n";
			file << "f " << vertexOffset + 4 << " " << vertexOffset + 6 << " " << vertexOffset + 7 << "\n";
			file << "f " << vertexOffset + 3 << " " << vertexOffset + 0 << " " << vertexOffset + 4 << "\n";
			file << "f " << vertexOffset + 4 << " " << vertexOffset + 7 << " " << vertexOffset + 3 << "\n";

			vertexOffset += 8;
		}
	}

	file.close();
	std::cout << "Exported scene to " << filename << std::endl;
}


/**
* Player paths orientation fix
*/
void RotatePlayers(std::vector<Player>& players) {
	glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(-1.0f, 0.0f, 0.0f));
	glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.025f));
	for (auto& p : players) {
		for (auto& pos : p.positions) {
			glm::vec4 transform = rotation * glm::vec4(pos, 1.0f);
			transform = scale * transform;
			pos = glm::vec3(transform);
		}
	}
	return;
}

/**
* Scrap positions not passing through the scene
*/
void FilterPositions(Player& player, BoundingBox& box) {
	std::vector<glm::vec3> newPositions;
	for (auto& pos : player.positions) {
		if (isInBounds(pos, box)) newPositions.push_back(pos);
	}
	player.positions = newPositions;
}

std::vector<Player> importPlayers(const std::string& path, BoundingBox& box) {
	std::cout << "importPlayers called" << std::endl;
	std::vector<Player> ret;
	py::object pyPlayers;
	pybind11::str paths = path;
	std::cout << "Calling python..." << std::endl;
	py::module loader = py::module::import("usd_loader"); try
	{
		pyPlayers = loader.attr("load_players")(paths);

		std::cout << "Python returned" << std::endl;
	}
	catch (const py::error_already_set& e)
	{
		std::cerr << "Python exception:\n"
			<< e.what()
			<< std::endl;
	}
	std::cout << "Processing data" << std::endl;
	py::list players = pyPlayers.cast<py::list>();
	int counter = 0;
	for (auto& player : players) {
		//std::cout << "Ch1" << std::endl;
		py::list positions = player.cast<py::list>();
		Player tmp;
		tmp.name = "player_" + std::to_string(counter);
		for (auto& position : positions) {
			//std::cout << "Ch2" << std::endl;
			py::tuple p = position.cast<py::tuple>();
			glm::vec3 v;
			v.x = p[0].cast<float>();
			v.y = p[1].cast<float>();
			v.z = p[2].cast<float>();
			tmp.positions.push_back(v);
		}
		ret.push_back(tmp);
		counter++;

	}

	py::object pyTransform;
	std::cout << "Calling python..." << std::endl;
	py::module loaderTr = py::module::import("usd_loader"); try
	{
		pyTransform = loaderTr.attr("get_players_transform")(paths);

		std::cout << "Python returned" << std::endl;
	}
	catch (const py::error_already_set& e)
	{
		std::cerr << "Python exception:\n"
			<< e.what()
			<< std::endl;
	}
	std::cout << "Processing data" << std::endl;

	counter = 0;
	py::list transform = pyTransform.cast<py::list>();
	for (auto& t : transform) {
		py::array_t<float> matrix = t.cast<py::array_t<float>>();
		auto mbuf = matrix.request();

		float* mdata = static_cast<float*>(mbuf.ptr);

		ret[counter].transform = glm::make_mat4(mdata);
		/*for (int i = 0; i < 4; i++) {
			std::cout << ret[counter].transform[i][0] << " " << ret[counter].transform[i][1] << " " << ret[counter].transform[i][2] << " " << ret[counter].transform[i][3] << std::endl;
		}
		std::cout << std::endl;*/
		counter++;
	}

	//ApplyTransformPlayers(ret);
	RotatePlayers(ret);
	for (auto& p : ret) {
		std::vector<glm::vec3> newPos;
		for (auto& v : p.positions) {
			if (isInBounds(v, box)) newPos.push_back(v);
		}
		p.positions = newPos;
	}
	std::vector<Player> newRet;
	counter = 0;
	for (auto& p : ret) {
		if (!p.positions.empty() && p.positions.size() > 1) {
			p.id = counter;
			newRet.push_back(p);
			counter++;
		}
	}
	/*for (auto& p : ret) {
		std::cout << p.name << std::endl;
		for (auto& v : p.positions) {
			std::cout << v.x << " " << v.y << " " << v.z << std::endl;
		}
		std::cout << std::endl;
	}*/

	for (auto& p : newRet) {
		glGenVertexArrays(1, &p.VAO);
		glGenBuffers(1, &p.VBO);

		glBindVertexArray(p.VAO);

		glBindBuffer(GL_ARRAY_BUFFER, p.VBO);
		glBufferData(GL_ARRAY_BUFFER, p.positions.size() * sizeof(glm::vec3), p.positions.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

		glEnableVertexAttribArray(0);
		glBindVertexArray(0);
	}

	return newRet;
}


void ExportPaths(std::string pName, std::vector<Player>& players, int nPaths, bool fps30) {

	std::ofstream file(pName);
	std::vector<Player> newPlayers = GetPathsCount(players, nPaths);
	int seconds = -1;
	for (auto& pl : newPlayers) {
		seconds = static_cast<int>(pl.positions.size()) > seconds ? static_cast<int>(pl.positions.size()) : seconds;
	}
	int fps = fps30 ? 30 : 60;
	int frameCount = seconds * fps;
	int objectCount = static_cast<int>(newPlayers.size());

	file << frameCount << "\n";
	file << objectCount << "\n";

	glm::vec3 playerUp = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 lastPos, nextPos, playerPos, playerTan, x, y, z;
	glm::mat4 out = glm::mat4(1.0f);

	for (int i = 0; i < seconds; ++i) {
		std::cout << "Exporting second " << i+1 << "/" << seconds << std::endl;
		for (int t = 0; t < fps; ++t) {
			float delta = static_cast<float>(t) / static_cast<float>(fps);
			for (size_t j = 0; j < newPlayers.size(); ++j) {
				//std::cout << "Player " << players[j].id << std::endl;
				int playerPoint = -1;
				if (static_cast<int>(newPlayers[j].positions.size()) < 2) {
					std::cout << "ERROR: Player " << newPlayers[j].id << " has less than 2 positions! Ordering invalidated!\n";
				}
				playerPoint = (i + newPlayers[j].pathStart) % (static_cast<int>(newPlayers[j].positions.size()) - 1);
				//std::cout << "Player point " << playerPoint << std::endl;
				//if (players[j].positions.empty()) std::cout << "Player detected empty!" << std::endl;
				lastPos = newPlayers[j].positions[playerPoint];
				nextPos = newPlayers[j].positions[playerPoint + 1];
				//std::cout << glm::to_string(lastPos) << " || " << glm::to_string(nextPos) << std::endl;
				playerPos = lastPos + delta * (nextPos - lastPos);
				//std::cout << glm::to_string(playerPos) << std::endl;
				/*playerTan = nextPos != lastPos ? glm::normalize(nextPos - lastPos) : glm::vec3(1.0f, 0.0f, 0.0f);
				x = playerTan;
				z = glm::normalize(glm::cross(x, playerUp));
				y = glm::cross(z, x);
				out[0] = glm::vec4(x, 0.0f);
				out[1] = glm::vec4(y, 0.0f);
				out[2] = glm::vec4(z, 0.0f);
				out[3] = glm::vec4(playerPos, 1.0f);*/
				glm::vec3 initialPos = newPlayers[j].positions[newPlayers[j].pathStart];

				glm::vec3 displacement = playerPos - initialPos;

				out = glm::translate(glm::mat4(1.f), displacement);
				file << out[0][0] << " " << out[0][1] << " " << out[0][2] << " " << out[0][3] << " "
					<< out[1][0] << " " << out[1][1] << " " << out[1][2] << " " << out[1][3] << " "
					<< out[2][0] << " " << out[2][1] << " " << out[2][2] << " " << out[2][3] << " "
					<< out[3][0] << " " << -out[3][2] << " " << out[3][1] << " " << out[3][3] << "\n";
			}
		}
	}
	if (!file) {
		throw std::runtime_error("Failed to write motion data");
	}
	file.close();
	pName = pName.substr(0, pName.find_last_of('\\') + 1) + "PlayerNames.txt";
	std::ofstream file_names(pName);
	file_names << "Copy the next line into the motion.select variable in OP\n";
	for (auto& p : newPlayers) {
		file_names << "\"player\_" << p.id << "\", ";
	}
	file_names << "\n";
	file_names.close();
	std::cout << "Copy the output of PlayerNames.txt into the motion.select variable in OP" << std::endl;
}
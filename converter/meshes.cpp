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
	std::string name;
	float mpu;
	glm::mat4 transform;
};

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

/*
* 
*
*
*
*
*
*
*
*
*
*
*/

void exportStats(const std::vector<Mesh>& meshes, BoundingBox& box, std::string fname, bool reduced) {
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
	int AABBcoverage = getAABBvector(segmentsAABB, meshes, box, xStep, yStep, zStep, segmentsRow);


	std::cout << "Initiating for loop..." << std::endl;
	for (auto& m : meshes) {
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
	file << "Scene occupancy ratio (segmentscontaining at least one triangle): " << occupancyRatio << "%" << std::endl;
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
	if (reduced) {
		file << "Bounding box values:" << std::endl;
		file << "X size: " << (box.Xplus - box.Xminus)/ (box.lim_Xplus - box.lim_Xminus) << std::endl;
		file << "X offset: " << (box.Xminus - box.lim_Xminus)/ (box.lim_Xplus - box.lim_Xminus) << std::endl;
		file << "Y size: " << (box.Yplus - box.Yminus) / (box.lim_Yplus - box.lim_Yminus) << std::endl;
		file << "Y offset: " << (box.Yminus - box.lim_Yminus) / (box.lim_Yplus - box.lim_Yminus) << std::endl;
		file << "Z size: " << (box.Zplus - box.Zminus) / (box.lim_Zplus - box.lim_Zminus) << std::endl;
		file << "Z offset: " << (box.Zminus - box.lim_Zminus) / (box.lim_Zplus - box.lim_Zminus) << std::endl;
	}
	else {
		file << "Default bounding box used for export" << std::endl;
	}

	file.close();
	std::cout << "Statistics exported to " << fname << std::endl;

}

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

		mesh.name = pyMesh["name"].cast<std::string>();
		std::size_t found = mesh.name.find("brush");
		if (found == std::string::npos && !(mesh.name.substr(0, 3) == "geo")) {
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

			mesh.transform = glm::make_mat4(mdata);

			verts += mesh.vertices.size();
			result.push_back(std::move(mesh));
		}else {
			//std::cout << "Brush located!" << std::endl;
		}

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

	/*box.lim_Xminus -= 1;
	box.lim_Yminus -= 1;
	box.lim_Zminus -= 1;
	box.lim_Xplus += 1;
	box.lim_Yplus += 1;
	box.lim_Zplus += 1;*/

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

void exportReduced(const std::vector<Mesh>& meshes, const std::string& filename, BoundingBox& box, bool stats) {
	std::vector<Mesh> reducedMeshes;

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
			reducedMeshes.push_back(reducedmesh);
		}
	}
	if (stats) {
		exportStats(reducedMeshes, box, filename, true);
		return;
	}

	std::ofstream file(filename);
	if (!file.is_open()) {
		throw std::runtime_error("Cannot open file: " + filename);
	}
	int vertexOffset = 1;
	for(auto& mesh : reducedMeshes) {
		file << "o " << mesh.name << "\n";

		for (const auto& v : mesh.vertices) {
			file << "v " << v.x << " " << v.y << " " << v.z << "\n";
		}

		for (int i = 0; i < mesh.triangles.size(); i += 3) {
			file << "f " << mesh.triangles[i] + vertexOffset << " " << mesh.triangles[i + 1] + vertexOffset << " " << mesh.triangles[i + 2] + vertexOffset << "\n";
		}

		vertexOffset += mesh.vertices.size();
	}
	file.close();
	std::cout << "Exported reduced scene to " << filename << std::endl;
}

void exportMeshes(const std::vector<Mesh>& meshes, const std::string& filename, BoundingBox& box, bool stats) {
	if (stats) {
		exportStats(meshes, box, filename, false);
		return;
	}

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


	file.close();
	std::cout << "Exported scene to " << filename << std::endl;
}




#pragma once
#include <glad/glad.h>
#include <vector>
#include <string>
#include <glm/gtc/type_ptr.hpp>
#include <limits>

/**
* Container for a single imported mesh
*/
struct Mesh {
	std::vector<glm::vec3> vertices;
	std::vector<uint32_t> triangles;
	/** Name extracted from the .usd/.usda scene */
	std::string name;
};

/**
* Container for a single player path
*/
struct Player {
	std::string name;
	std::vector<glm::vec3> positions;
	glm::mat4 transform;
	GLuint VAO;
	GLuint VBO;
	int id;
};

/**
* Container for a version of imported meshes used for rendering
*/
struct RenderMesh {
	std::vector<glm::vec3> vertices;
	std::vector<uint32_t> triangles;
	std::vector<glm::vec3> normals;
	GLuint VAO;
	GLuint VBO;
	GLuint EBO;
};

/**
* Bounding box used to crop the scene, player/camera paths and statistics/objects export
*/
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

/**
* Imports .usd/.usda scenes into program
* 
* Scenes get filtered for geometry objects and rotated and scaled accordingly
* 
* @param paths List of all directory paths to be imported
* @param renderMesh Container for a copy of the scene used for rendering
* @param randSampling Introduces random triangle filtering, higher value filters more triangles
* @param box Container for the scene bounding box parametres
* @return Containers with loaded and transformed meshes
*/
std::vector<Mesh> importMesh(const std::vector<std::string>& paths, std::vector<RenderMesh>& renderMesh, int randSampl, BoundingBox& box);

/**
* Exports the entire imported scene in the .obj format
* 
* @param meshes Containers with meshes included in the scene 
* @param filename Name of the export file
*/
void exportMeshes(const std::vector<Mesh>& meshes, const std::string& filename);

/**
* Exports the imported scene in the .obj format according to bounding box limitations
*
* @param meshes Containers with meshes included in the scene
* @param filename Name of the export file
* @param box Bounding box limiting the dimensions of the scene
*/
void exportReduced(const std::vector<Mesh>& meshes, const std::string& filename, BoundingBox& box);


/**
* Exports the path of the selected player as a sequence of 3D points in the .txt format
* 
* @param player Container of the selected player
* @param box Bounding box limiting the length of the player path
* @param path Name of the export file
*/
void exportPlayer(const Player& player, BoundingBox& box, std::string path);

/**
* Imports all player paths found in the breadcrumbs.usd file, crops them to the scene and prepares them for rendering
* 
* @param path File location
* @param box Bounding box used to crop player paths
* 
* @return Vector of player containers
*/
std::vector<Player> importPlayers(const std::string& path, BoundingBox& box);

/**
* Exports camera points as a sequence of 3D points into .txt format
* 
* @param points sequence of points to export
* @param filename Name of the export file
*/
void exportCamera(const std::vector<glm::vec3>& points, std::string filename);

/**
* Exports scene statistics into .txt format
* 
* @param meshes Containers with meshes included in the scene
* @param box Bounding box used to crop the scene if requested
* @param fname Name of the export file
* @param reduced option to crop the statistics to scene inside the bounding box
*/
void exportStats(const std::vector<Mesh>& meshes, std::string fname, bool reduced, BoundingBox& box);


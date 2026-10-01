// converter.cpp: Definuje vstupní bod pro aplikaci.
//
#include "converter.h"
#include <glad/glad.h>
#include "meshes.h"
#include "camera.h"
#include <iostream>
#include <GLFW/glfw3.h>
#include <string>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <pybind11/embed.h>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <windows.h>
#include <commdlg.h>
#include <chrono>


using clockupdate = std::chrono::steady_clock;

enum CamState {
	STATIONARY,
	POLY,
	CURVE
};

GLuint createShader() {
	const char* vs = R"(
        #version 330 core
        layout (location = 0) in vec3 position;
        uniform mat4 MVP;
		uniform mat4 model;
		out vec3 vPos;
        void main() {
            vPos = vec3(model * vec4(position, 1.0));
			gl_Position = MVP * vec4(position, 1.0);
        }
    )";

	const char* fs = R"(
        #version 330 core
        out vec4 FragColor;
		in vec3 vPos;
		uniform bool wireframe;
		uniform vec3 maxBounds;
		uniform vec3 minBounds;
		uniform bool triangles;

        void main() {
			vec3 color;
			if(!triangles){
				FragColor = vec4(0.0, 0.0, 1.0, 1.0);
			}else{
				if(wireframe){
					if(vPos.x > maxBounds.x || vPos.x < minBounds.x ||
						vPos.y > maxBounds.y || vPos.y < minBounds.y ||
						vPos.z > maxBounds.z || vPos.z < minBounds.z){

						color = vec3(1.0, 0.0, 0.0);
					}else{
						color = vec3(1.0);
					}
				}else{
					vec3 dx = dFdx(vPos);
					vec3 dy = dFdy(vPos);

					vec3 normal = normalize(cross(dx, dy));
					vec3 lightDir = normalize(vec3(1, 1, 1));
					float diff = max(dot(normal, lightDir), 0.0);

					if(vPos.x > maxBounds.x || vPos.x < minBounds.x ||
						vPos.y > maxBounds.y || vPos.y < minBounds.y ||
						vPos.z > maxBounds.z || vPos.z < minBounds.z){

						color = vec3(1.0, 0.0, 0.0) * diff + vec3(0.2);
					}else{
						color = vec3(0.7) * diff + vec3(0.2);
					}
				}
				FragColor = vec4(color, 1.0);
			}
		}
    )";

	auto compile = [](GLenum type, const char* src) {
		GLuint s = glCreateShader(type);
		glShaderSource(s, 1, &src, nullptr);
		glCompileShader(s);
		return s;
		};

	GLuint v = compile(GL_VERTEX_SHADER, vs);
	GLuint f = compile(GL_FRAGMENT_SHADER, fs);

	GLuint prog = glCreateProgram();
	glAttachShader(prog, v);
	glAttachShader(prog, f);
	glLinkProgram(prog);

	glDeleteShader(v);
	glDeleteShader(f);

	return prog;
}

std::string saveFileObj() {
	char filename[MAX_PATH] = "";

	OPENFILENAMEA ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFilter = "OBJ Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
	ofn.lpstrDefExt = "obj";

	if (GetSaveFileNameA(&ofn)) {
		return std::string(filename);
	}

	return ""; // user canceled
}

std::string saveFileTxt() {
	char filename[MAX_PATH] = "";

	OPENFILENAMEA ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFilter = "TXT Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
	ofn.lpstrDefExt = "txt";

	if (GetSaveFileNameA(&ofn)) {
		return std::string(filename);
	}

	return ""; // user canceled
}

std::string openFile() {
	char filename[MAX_PATH] = "";

	OPENFILENAMEA ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFilter = "USD Files (*.usd;*.usda)\0*.usd;*.usda\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

	if (GetOpenFileNameA(&ofn)) {
		return std::string(filename);
	}
	return "";
}

std::string getName(const std::string& path) {
	size_t pos = path.find_last_of("/\\");
	if (pos == std::string::npos) return path;
	return path.substr(pos + 1);
}

float GetTimeDelta(auto &lastUpdate, float speed) {
	auto now = clockupdate::now();

	float delta =
		std::chrono::duration<float>(now - lastUpdate).count();

	lastUpdate = now;

	return delta * speed;
}

int main()
{
	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW!" << std::endl;
		return -1;
	}
	pybind11::scoped_interpreter guard{};
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);

	GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Converter Viewer", nullptr, nullptr);
	if (!window) {
		std::cerr << "Failed to create a window!" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	glEnable(GL_DEPTH_TEST);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 130");

	GLuint shader = createShader();

	
	// UI interactions
	std::vector<std::string> paths; // list of paths of scenes included to be imported
	float moveSpeed = 5.0f; // pan movement speed modifier
	float zoomSpeed = 5.0f; // zoom speed modifier
	bool wireframe = false; // wireframe rendering option switch
	int selectedPath = -1; // selected path to .usd scene
	int selectedPlayer = -1; // selected player path from the list
	int randomSampling = 1; // random sampling option variable
	uint32_t nFaces = 0; // total number of triangles counter
	bool showPlayers = true; // switch for player paths rendering
	int selectedCamPos = -1; // selected camera position (glm::vec3) on its path
	bool showCamera = true; // switch whether camera path should be rendered
	bool curves = false; // switch between staight lines and curvy camera movement
	float xMinWorld = 0, xMaxWorld = 1, yMinWorld = 0, yMaxWorld = 1, zMinWorld = 0, zMaxWorld = 1; // variables for the bounding box manipulation


	// rendering data
	std::vector<RenderMesh> renderMesh; //contains rendering meshes
	std::vector<Player> players; // list of all player paths
	Camera cameraPoints; // list of all camera points (extracted from selected player)
	std::vector<Mesh> meshes; //contains export meshes
	BoundingBox box;

	// camera variables
	GLuint camVAO;
	GLuint camVBO;
	GLuint pointVAO;
	GLuint pointVBO; // buffers for camera showing
	glm::vec3 direction;
	glm::vec3 target = glm::vec3(0.0f);
	glm::vec3 up; // vectors for camera setup

	// camera walkthrough movement variables
	auto lastUpdate = clockupdate::now(); // real time delta for movement
	int nextPoint = -1; // next anchor point for camera movement
	float timeDelta = -1; // last update delta for movement
	float viewT = -1;
	enum CamState cameraState = STATIONARY; // camera mode switch
	std::vector<ArcLengthTable> pathLUT; // // experimental camera movement implementation
	glm::vec3 lastPos; // last position of the moving camera


	// mouse movement variables
	float rotX = 0.0f, rotY = 0.0f; // camera variables
	float distance = 5.0f; // camera movement variable
	float yaw = 0.0f; // camera rotation variable
	float pitch = 0.0f; // camera rotation variable
	glm::vec2 pan(0.0f); // camera rotation variable


	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImVec2 display = ImGui::GetIO().DisplaySize;
		
		if (ImGui::IsKeyPressed(ImGuiKey_C) && cameraState != STATIONARY) {
			cameraState = STATIONARY;
		}

		if (cameraState == STATIONARY) {
			// window 1 setup - general info and movement adjustments
			ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
			ImGui::SetNextWindowSize(ImVec2(500.0f / 1920.0f * display.x, 110.0f / 1080.0f * display.y), ImGuiCond_Once);
			ImGui::Begin("Converter Tools");
			ImGui::Checkbox("Wireframe", &wireframe);
			ImGui::SameLine();
			ImGui::Text("Number of faces: %d", nFaces);
			ImGui::SliderFloat("Move speed", &moveSpeed, 1.0f, 100.0f);
			ImGui::SliderFloat("Zoom speed", &zoomSpeed, 1.0f, 100.0f);
			ImGui::SliderInt("Random Sampling", &randomSampling, 1, 10);
			ImGui::Text("Drag mouse to rotate");

			ImGui::End();
		}

		if (cameraState == STATIONARY) {
			// window 2 setup - scene paths management and import
			ImGui::SetNextWindowPos(ImVec2(0, 110.0f / 1080.0f * display.y), ImGuiCond_Once);
			ImGui::SetNextWindowSize(ImVec2(300.0f / 1920.0f * display.x, 150.0f / 1080.0f * display.y), ImGuiCond_Once);
			ImGui::Begin("Paths management");

			if (ImGui::Button("Add path")) {
				std::string selectedPath = openFile();

				if (!selectedPath.empty()) {
					std::string path = selectedPath;
					std::cout << "Selected: " << path << std::endl;
					paths.push_back(path);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove path") && selectedPath != -1) {
				paths.erase(paths.begin() + selectedPath);
				selectedPath = -1;
			}
			ImGui::SameLine();
			if (ImGui::Button("Load and render") && !paths.empty()) {
				nFaces = 0;
				BoundingBox tmp;
				xMinWorld = 0, xMaxWorld = 1, yMinWorld = 0, yMaxWorld = 1, zMinWorld = 0, zMaxWorld = 1;
				float moveSpeed = 5.0f;
				float zoomSpeed = 5.0f;
				box = tmp;
				meshes = importMesh(paths, renderMesh, randomSampling, box);
				if (!meshes.empty() && !renderMesh.empty()) {
					std::cout << "Mesh loaded and rotated" << std::endl;
				}
				else {
					std::cerr << "Loading failed" << meshes.size() << " " << renderMesh.size() << std::endl;
				}
				for (auto& mesh : meshes) {
					nFaces += (mesh.triangles.size() / 3);
				}
				std::cout << "Bounding Box:" << std::endl;
				std::cout << "+X: " << box.Xplus << std::endl;
				std::cout << "-X: " << box.Xminus << std::endl;
				std::cout << "+Y: " << box.Yplus << std::endl;
				std::cout << "-Y: " << box.Yminus << std::endl;
				std::cout << "+Z: " << box.Zplus << std::endl;
				std::cout << "-Z: " << box.Zminus << std::endl;
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear View")) {
				meshes.clear();
				renderMesh.clear();
			}
			for (int i = 0; i < paths.size(); i++) {
				std::string name = getName(paths[i]);

				if (ImGui::Selectable(name.c_str(), selectedPath == i)) {
					selectedPath = i;
				}
			}
			ImGui::End();
		}


		if (cameraState == STATIONARY) {
			// window 3 setup - buttons for exports of stats and objects
			ImGui::SetNextWindowPos(ImVec2(1720.0f / 1920.0f * display.x, 0), ImGuiCond_Once);
			ImGui::SetNextWindowSize(ImVec2(200.0f / 1920.0f * display.x, 80.0f / 1080.0f * display.y), ImGuiCond_Once);
			ImGui::Begin("Export options");
			if (ImGui::Button("Export models") && !meshes.empty()) {
				std::string savePath = saveFileObj();

				if (!savePath.empty()) {
					if (xMaxWorld != 1 || xMinWorld != 0 ||
						yMaxWorld != 1 || yMinWorld != 0 ||
						zMaxWorld != 1 || zMinWorld != 0) {
						exportReduced(meshes, savePath, box);
					}
					else {
						exportMeshes(meshes, savePath);
					}
				}
			}
			if (ImGui::Button("Export stats") && !meshes.empty()) {
				std::string savePath = saveFileTxt();

				if (!savePath.empty()) {
					if (xMaxWorld != 1 || xMinWorld != 0 ||
						yMaxWorld != 1 || yMinWorld != 0 ||
						zMaxWorld != 1 || zMinWorld != 0) {
						exportStats(meshes, savePath, false, box);
					}
					else {
						exportStats(meshes, savePath, true, box);
					}
				}
			}

			ImGui::End();
		}


		if (cameraState == STATIONARY) {
			// window 4 setup - bounding box size manipulation
			ImGui::SetNextWindowPos(ImVec2(0, 880.0f / 1080.0f * display.y), ImGuiCond_Once);
			ImGui::SetNextWindowSize(ImVec2(display.x, 200.0f / 1080.0f * display.y), ImGuiCond_Once);
			ImGui::Begin("Bounding box");
			ImGui::SliderFloat("X size", &xMaxWorld, 0.0f, 1.0f);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(50.0f);
			ImGui::InputFloat("##button1", &xMaxWorld, 0.0f, 1.0f);
			ImGui::SliderFloat("X offset", &xMinWorld, 0.0f, 1.0f);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(50.0f);
			ImGui::InputFloat("##button2", &xMinWorld, 0.0f, 1.0f);
			ImGui::SliderFloat("Y size", &yMaxWorld, 0.0f, 1.0f);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(50.0f);
			ImGui::InputFloat("##button3", &yMaxWorld, 0.0f, 1.0f);
			ImGui::SliderFloat("Y offset", &yMinWorld, 0.0f, 1.0f);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(50.0f);
			ImGui::InputFloat("##button4", &yMinWorld, 0.0f, 1.0f);
			ImGui::SliderFloat("Z size", &zMaxWorld, 0.0f, 1.0f);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(50.0f);
			ImGui::InputFloat("##button5", &zMaxWorld, 0.0f, 1.0f);
			ImGui::SliderFloat("Z offset", &zMinWorld, 0.0f, 1.0f);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(50.0f);
			ImGui::InputFloat("##button6", &zMinWorld, 0.0f, 1.0f);


			ImGui::End();
		}



		if (cameraState == STATIONARY) {
			// window 5 setup - player paths import and showcase
			ImGui::SetNextWindowPos(ImVec2(1790.0f / 1920.0f * display.x, 80.0f / 1080.0f * display.y), ImGuiCond_Once);
			ImGui::SetNextWindowSize(ImVec2(130 / 1920.0f * display.x, (1080.0f - 280.0f) / 1080.0f * display.y), ImGuiCond_Once);
			ImGui::Begin("Player paths");
			ImGui::Checkbox("Show Paths", &showPlayers);
			if (ImGui::Button("Load players")) {
				std::string selectedPath = openFile();

				if (!selectedPath.empty()) {
					std::string path = selectedPath;
					players = importPlayers(path, box);
				}
			}
			if (ImGui::Button("Remove selection") && selectedPlayer != -1) {
				selectedPlayer = -1;
			}
			if (ImGui::Button("Export selected path") && selectedPlayer != -1) {
				std::string savePath = saveFileTxt();
				if (!savePath.empty())
					exportPlayer(players[selectedPlayer], box, savePath);
			}
			ImGui::Separator();
			ImGui::BeginChild("Players", ImVec2(0, 0), ImGuiChildFlags_Borders);
			for (int i = 0; i < players.size(); i++) {
				std::string name = players[i].name;
				if (ImGui::Selectable(name.c_str(), selectedPlayer == i)) {
					selectedPlayer = i;
				}
			}
			ImGui::EndChild();
			ImGui::End();
		}


		if (cameraState == STATIONARY) {
			//window 6 setup - camera path import, management and adjustment
			ImGui::SetNextWindowPos(ImVec2(1520.0f / 1920.0f * display.x, 80.0f / 1080.0f * display.y), ImGuiCond_Once);
			ImGui::SetNextWindowSize(ImVec2(270.0f / 1920.0f * display.x, (1080.0f - 280.0f) / 1080.0f * display.y), ImGuiCond_Once);
			ImGui::Begin("Camera Path");

			ImGui::Checkbox("Show path", &showCamera);
			ImGui::SameLine();
			ImGui::Checkbox("Curves path", &curves);
			if (ImGui::Button("Import from selected player") && selectedPlayer != -1) {
				cameraPoints.positions = players[selectedPlayer].positions;
				cameraPoints.origSize = cameraPoints.positions.size();
				cameraPoints.original = std::vector<bool>(cameraPoints.positions.size(), true);
				for (auto& p : cameraPoints.positions) {
					p += glm::vec3(0.0f, 1.5f, 0.0f);

				}
				glGenVertexArrays(1, &camVAO);
				glGenBuffers(1, &camVBO);

				glBindVertexArray(camVAO);

				glBindBuffer(GL_ARRAY_BUFFER, camVBO);
				glBufferData(GL_ARRAY_BUFFER, cameraPoints.positions.size() * sizeof(glm::vec3), cameraPoints.positions.data(), GL_DYNAMIC_DRAW);
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

				glEnableVertexAttribArray(0);
				glBindVertexArray(0);


				glGenVertexArrays(1, &pointVAO);
				glGenBuffers(1, &pointVBO);
			}

			ImGui::SameLine();

			if (ImGui::Button("Remove point") && selectedCamPos != -1) {
				cameraPoints.positions.erase(cameraPoints.positions.begin() + selectedCamPos);
				cameraPoints.original.erase(cameraPoints.original.begin() + selectedCamPos);
				cameraPoints.origSize--;
			}

			if (ImGui::Button("Add point before") && selectedCamPos != -1) {
				glm::vec3 newPos = glm::vec3(0.0f);
				if (selectedCamPos == 0) {
					newPos.x -= 1.0f;

				}
				else {
					newPos = cameraPoints.positions[selectedCamPos - 1] + 0.5f * (cameraPoints.positions[selectedCamPos] - cameraPoints.positions[selectedCamPos - 1]);
				}
				cameraPoints.positions.emplace(cameraPoints.positions.begin() + selectedCamPos, newPos);
				cameraPoints.original.emplace(cameraPoints.original.begin() + selectedCamPos, false);

			}

			ImGui::SameLine();
			if (ImGui::Button("Add point after") && selectedCamPos != -1) {
				glm::vec3 newPos = glm::vec3(0.0f);
				if (selectedCamPos == cameraPoints.positions.size()) {
					newPos.x += 1.0f;
				}
				else {
					newPos = cameraPoints.positions[selectedCamPos] + 0.5f * (cameraPoints.positions[selectedCamPos + 1] - cameraPoints.positions[selectedCamPos]);
				}
				cameraPoints.positions.emplace(cameraPoints.positions.begin() + selectedCamPos + 1, newPos);
				cameraPoints.original.emplace(cameraPoints.original.begin() + selectedCamPos + 1, false);
			}
			if (ImGui::Button("Export camera") && cameraPoints.positions.size() > 0) {
				std::string savePath = saveFileTxt();
				if (!savePath.empty()) exportCamera(cameraPoints.positions, savePath);
			}

			ImGui::SameLine();
			if (ImGui::Button("Move camera") && cameraState == STATIONARY && cameraPoints.positions.size() > 0) {
				cameraState = curves ? CURVE : POLY;
				pathLUT = cameraState == CURVE ? GetPathCurves(cameraPoints) : GetPathLines(cameraPoints);
				nextPoint = selectedCamPos > 0 ? selectedCamPos : 0;
				timeDelta = 0;
				std::cout << "LUT has " << pathLUT.size() << " segments" << std::endl;
				lastUpdate = clockupdate::now();
				lastPos = cameraPoints.positions[nextPoint];
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel movement") && cameraState != STATIONARY) {
				cameraState = STATIONARY;
			}
			ImGui::Separator();
			ImGui::BeginChild("Camera points", ImVec2(0, 0), ImGuiChildFlags_Borders);
			int pointCounter = 0;
			for (int i = 0; i < cameraPoints.positions.size(); i++) {
				std::string p;
				if (cameraPoints.original[i]) {
					p = "point " + std::to_string(pointCounter) + ": " + std::to_string(cameraPoints.positions[i].x) + " " + std::to_string(cameraPoints.positions[i].y) + " " + std::to_string(cameraPoints.positions[i].z);
				}
				else {
					p = "point new " + std::to_string(pointCounter) + ": " + std::to_string(cameraPoints.positions[i].x) + " " + std::to_string(cameraPoints.positions[i].y) + " " + std::to_string(cameraPoints.positions[i].z);
				}

				if (ImGui::Selectable(p.c_str(), selectedCamPos == i)) {
					selectedCamPos = i;
				}
				pointCounter++;
			}
			ImGui::EndChild();
			ImGui::End();
		}


		if (cameraState == STATIONARY) {
			// window 7 setup - editor for a single point on the camera path
			ImGui::SetNextWindowPos(ImVec2(1520.0f / 1920.0f * display.x, 0), ImGuiCond_Once);
			ImGui::SetNextWindowSize(ImVec2(200.0f / 1920.0f * display.x, 80.0f / 1080.0f * display.y), ImGuiCond_Once);
			ImGui::Begin("Point Editor");

			float x = selectedCamPos == -1 ? 0 : cameraPoints.positions[selectedCamPos].x;
			float y = selectedCamPos == -1 ? 0 : cameraPoints.positions[selectedCamPos].y;
			float z = selectedCamPos == -1 ? 0 : cameraPoints.positions[selectedCamPos].z;

			ImGui::InputFloat("X  ", &x);
			ImGui::SameLine();
			if (ImGui::Button("+##button1") && selectedCamPos != -1) {
				x += 1.0f;
			}
			ImGui::SameLine();
			if (ImGui::Button("-##button2") && selectedCamPos != -1) {
				x -= 1.0f;
			}

			ImGui::InputFloat("Y  ", &y);
			ImGui::SameLine();
			if (ImGui::Button("+##button3") && selectedCamPos != -1) {
				y += 1.0f;
			}
			ImGui::SameLine();
			if (ImGui::Button("-##button4") && selectedCamPos != -1) {
				y -= 1.0f;
			}
			ImGui::InputFloat("Z  ", &z);
			ImGui::SameLine();
			if (ImGui::Button("+##button5") && selectedCamPos != -1) {
				z += 1.0f;
			}
			ImGui::SameLine();
			if (ImGui::Button("-##button6") && selectedCamPos != -1) {
				z -= 1.0f;
			}

			if (selectedCamPos > -1) {
				cameraPoints.positions[selectedCamPos].x = x;
				cameraPoints.positions[selectedCamPos].y = y;
				cameraPoints.positions[selectedCamPos].z = z;
			}
			ImGui::End();
		}



		// update bounding box parametres
		box.Xminus = box.lim_Xminus + xMinWorld * (box.lim_Xplus - box.lim_Xminus);
		box.Yminus = box.lim_Yminus + yMinWorld * (box.lim_Yplus - box.lim_Yminus);
		box.Zminus = box.lim_Zminus + zMinWorld * (box.lim_Zplus - box.lim_Zminus);

		box.Xplus = box.Xminus + xMaxWorld * (box.lim_Xplus - box.lim_Xminus);
		box.Yplus = box.Yminus + yMaxWorld * (box.lim_Yplus - box.lim_Yminus);
		box.Zplus = box.Zminus + zMaxWorld * (box.lim_Zplus - box.lim_Zminus);


		// updata camera based on mouse inputs - only if stationary
		if (cameraState == STATIONARY) {
			direction.x = cos(glm::radians(pitch)) * sin(glm::radians(yaw));
			direction.y = sin(glm::radians(pitch));
			direction.z = cos(glm::radians(pitch)) * cos(glm::radians(yaw));

			glm::vec3 worldUp = glm::vec3(0, 1, 0);
			glm::vec3 right = glm::normalize(glm::cross(direction, worldUp));
			up = glm::normalize(glm::cross(right, direction));

			if (!ImGui::GetIO().WantCaptureMouse &&
				ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {

				ImVec2 d = ImGui::GetIO().MouseDelta;
				yaw -= d.x * 0.5f;
				pitch -= d.y * 0.5f;

				pitch = glm::clamp(pitch, -89.0f, 89.0f);
			}

			distance -= ImGui::GetIO().MouseWheel * (zoomSpeed / 5);
			distance = glm::clamp(distance, 1.0f, 1000.0f);

			if (!ImGui::GetIO().WantCaptureMouse &&
				ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {

				ImVec2 d = ImGui::GetIO().MouseDelta;

				float panSpeed = distance * moveSpeed / 1000;

				target -= right * d.x * panSpeed;
				target += up * d.y * panSpeed;
			}
		}


		ImGui::Render();


		int w, h;
		glfwGetFramebufferSize(window, &w, &h);
		glViewport(0, 0, w, h);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(0.2f, 0.2f, 0.1f, 1.0f);
		if (!meshes.empty()) {
			glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);

			if (cameraState != STATIONARY) {
				timeDelta += GetTimeDelta(lastUpdate, 0.75f);
				//std::cout << timeDelta << std::endl;
				if (timeDelta >= 1.0f) {
					while (timeDelta >= 1.0f) {
						timeDelta -= 1.0f;
						nextPoint++;
						//std::cout << "Entering segment " << nextPoint << std::endl;
					}

					if (nextPoint >= static_cast<int>(pathLUT.size())) {
						cameraState = STATIONARY;
						nextPoint = static_cast<int>(pathLUT.size()) - 1;
						timeDelta = 0.0f;
					}
				}
			}

			glm::vec3 cameraPos;
			glm::mat4 view;
			glm::mat4 model;
			glm::mat4 proj;
			glm::vec3 targetTMP;
			switch (cameraState)
			{
			case STATIONARY:
				cameraPos = target - direction * distance;
				view = glm::lookAt(cameraPos, target, up);
				model = glm::mat4(1.0f);
				proj = glm::infinitePerspective(
					glm::radians(45.0f),
					(float)w / h,
					0.1f
				);
				break;
			case POLY:
				cameraPos = EvaluateArcLength(pathLUT[nextPoint].samples, timeDelta, pathLUT[nextPoint].normalizedDistance);

				if (nextPoint >= 0) {
					const auto& previousPath = pathLUT[nextPoint];

					if (!previousPath.samples.empty()) {
						glm::vec3 previousEnd = previousPath.samples[0];

						targetTMP = cameraPos - (previousEnd - cameraPos);
					}
					else {
						targetTMP = cameraPos + glm::vec3(0.0f, 0.0f, -1.0f);
					}
				}
				else {
					// No previous path exists.
					std::cout << "No path!" << std::endl;
					targetTMP =
						cameraPos + glm::vec3(0.0f, 0.0f, -1.0f);
				}

				view = glm::lookAt(
					cameraPos,
					targetTMP,
					glm::vec3(0.0f, 1.0f, 0.0f));

				model = glm::mat4(1.0f);

				proj = glm::infinitePerspective(
					glm::radians(45.0f),
					static_cast<float>(w) / h,
					0.1f
				);

				break;
			case CURVE:
				viewT = timeDelta+0.5f;
				while (viewT >= 1.0f) {
					//std::cout << "Lowering..." << std::endl;
					viewT -= 1.0f;
				}
				cameraPos = EvaluateArcLength(pathLUT[nextPoint].samples, timeDelta, pathLUT[nextPoint].normalizedDistance);
				//targetTMP = cameraPos + glm::vec3(0.0f, 0.0f, 1.0f);
				if (viewT < timeDelta) {
					//std::cout << "Event triggered" << std::endl;
					targetTMP = nextPoint + 1 >= pathLUT.size() - 1 ? cameraPoints.positions[cameraPoints.positions.size() - 1] : EvaluateArcLength(pathLUT[nextPoint + 1].samples, viewT, pathLUT[nextPoint+1].normalizedDistance);
				}
				else {
					targetTMP = EvaluateArcLength(pathLUT[nextPoint].samples, viewT, pathLUT[nextPoint].normalizedDistance);
				}
				//std::cout << "Tangent: " << targetTMP.x << " " << targetTMP.y << " " << targetTMP.z << std::endl;
				//std::cout << "Camera: " << cameraPos.x << " " << cameraPos.y << " " << cameraPos.z << std::endl;
				//std::cout << "Tangent: " << targetTMP.x << " " << targetTMP.y << " " << targetTMP.z << std::endl;
				view = glm::lookAt(
					cameraPos,
					targetTMP,
					glm::vec3(0.0f, 1.0f, 0.0f));

				model = glm::mat4(1.0f);

				proj = glm::infinitePerspective(
					glm::radians(45.0f),
					static_cast<float>(w) / h,
					0.1f
				);
				lastPos = cameraPos;
				break;
			default:
				std::cout << "ERROR: Undefined camera state!" << std::endl;
				break;
			}
			glm::mat4 MVP = proj * view * model;
			glUseProgram(shader);
			glUniformMatrix4fv(glGetUniformLocation(shader, "MVP"),
				1, GL_FALSE, glm::value_ptr(MVP));
			glUniformMatrix4fv(glGetUniformLocation(shader, "model"),
				1, GL_FALSE, glm::value_ptr(model));
			glUniform1i(glGetUniformLocation(shader, "wireframe"),
				wireframe);

			glUniform1i(glGetUniformLocation(shader, "triangles"),
				true);

			glUniform3f(glGetUniformLocation(shader, "maxBounds"),
				box.Xplus, box.Yplus, box.Zplus);
			glUniform3f(glGetUniformLocation(shader, "minBounds"),
				box.Xminus, box.Yminus, box.Zminus);

			for (auto& mesh : renderMesh) {
				glBindVertexArray(mesh.VAO);
				glDrawElements(GL_TRIANGLES, mesh.triangles.size(), GL_UNSIGNED_INT, 0);
			}
		}
		if (!players.empty() && showPlayers && cameraState == STATIONARY) {
			glm::vec3 cameraPos = target - direction * distance;
			glm::mat4 view = glm::lookAt(cameraPos, target, up);
			glm::mat4 model = glm::mat4(1.0f);
			glm::mat4 proj = glm::infinitePerspective(
				glm::radians(45.0f),
				(float)w / h,
				0.1f
			);
			glm::mat4 MVP = proj * view * model;
			glUseProgram(shader);
			glUniformMatrix4fv(glGetUniformLocation(shader, "MVP"),
				1, GL_FALSE, glm::value_ptr(MVP));
			glUniformMatrix4fv(glGetUniformLocation(shader, "model"),
				1, GL_FALSE, glm::value_ptr(model));

			glUniform1i(glGetUniformLocation(shader, "triangles"),
				false);
			glLineWidth(1.0f);
			for (auto& p : players) {
				if (selectedPlayer == -1 || selectedPlayer == p.id) {
					glBindVertexArray(p.VAO);
					glDrawArrays(GL_LINE_STRIP, 0, p.positions.size());
					glBindVertexArray(0);
				}
			}
		}
		if (showCamera && !cameraPoints.positions.empty() && cameraState == STATIONARY) {
			glBindBuffer(GL_ARRAY_BUFFER, camVBO);
			glBufferData(GL_ARRAY_BUFFER, cameraPoints.positions.size() * sizeof(glm::vec3), cameraPoints.positions.data(), GL_DYNAMIC_DRAW);
			glm::vec3 cameraPos = target - direction * distance;
			glm::mat4 view = glm::lookAt(cameraPos, target, up);
			glm::mat4 model = glm::mat4(1.0f);
			glm::mat4 proj = glm::infinitePerspective(
				glm::radians(45.0f),
				(float)w / h,
				0.1f
			);
			glm::mat4 MVP = proj * view * model;
			glUseProgram(shader);
			glUniformMatrix4fv(glGetUniformLocation(shader, "MVP"),
				1, GL_FALSE, glm::value_ptr(MVP));
			glUniformMatrix4fv(glGetUniformLocation(shader, "model"),
				1, GL_FALSE, glm::value_ptr(model));

			glUniform1i(glGetUniformLocation(shader, "triangles"),
				false);
			glLineWidth(1.0f);
			glBindVertexArray(camVAO);
			glDrawArrays(GL_LINE_STRIP, 0, cameraPoints.positions.size());

			if (selectedCamPos > -1) {
				glPointSize(6.0f);
				glDrawArrays(GL_POINTS, selectedCamPos, 1);
			}

			glBindVertexArray(0);
		}
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();

	return 0;
}

// converter.cpp: Definuje vstupní bod pro aplikaci.
//
#include "converter.h"
#include <glad/glad.h>
#include "meshes.h"
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

	std::vector<std::string> paths;
	

	std::vector<Mesh> meshes;
	std::vector<RenderMesh> renderMesh;
	BoundingBox box;
	float rotX = 0.0f, rotY = 0.0f;
	float moveSpeed = 5.0f;
	float zoomSpeed = 5.0f;
	bool wireframe = false;
	glm::vec3 target = glm::vec3(0.0f);
	float distance = 5.0f;
	float yaw = 0.0f;
	float pitch = 0.0f;
	glm::vec2 pan(0.0f);
	int selectedPath = -1;
	int randomSampling = 1;
	uint32_t nFaces = 0;
	bool stats = false;
	bool statsOnly = false;
	bool playerPaths = false;
	std::vector<Player> players;

	float xMinWorld = 0, xMaxWorld = 1, yMinWorld = 0, yMaxWorld = 1, zMinWorld = 0, zMaxWorld = 1;

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImVec2 display = ImGui::GetIO().DisplaySize;
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(500.0f / 1920.0f * display.x, 220.0f / 1080.0f * display.y), ImGuiCond_Once);
		ImGui::Begin("Converter Tools");
		ImGui::Checkbox("Wireframe", &wireframe);
		ImGui::Text("Number of faces: %d", nFaces);
		ImGui::SliderFloat("Move speed", &moveSpeed, 1.0f, 100.0f);
		ImGui::SliderFloat("Zoom speed", &zoomSpeed, 1.0f, 100.0f);
		ImGui::SliderInt("Random Sampling", &randomSampling, 1, 10);
		ImGui::Text("Drag mouse to rotate");
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
		if (ImGui::Button("Clear View")) {
			meshes.clear();
			renderMesh.clear();
		}
		ImGui::End();


		ImGui::SetNextWindowPos(ImVec2(0, 220.0f / 1080.0f * display.y), ImGuiCond_Once);
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
		if (ImGui::Button("Remove path") && selectedPath != -1) {
			paths.erase(paths.begin() + selectedPath);
			selectedPath = -1;
		}
		for (int i = 0; i < paths.size(); i++) {
			std::string name = getName(paths[i]);

			if (ImGui::Selectable(name.c_str(), selectedPath == i)) {
				selectedPath = i;
			}
		}

		ImGui::End();


		ImGui::SetNextWindowPos(ImVec2(1720.0f / 1920.0f * display.x, 0), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(200.0f / 1920.0f * display.x, 150.0f / 1080.0f * display.y), ImGuiCond_Once);
		ImGui::Begin("Export options");
		if (ImGui::Button("Export models") && !meshes.empty()) {
			std::string savePath = saveFileObj();

			if (!savePath.empty()) {
				if (xMaxWorld != 1 || xMinWorld != 0 ||
					yMaxWorld != 1 || yMinWorld != 0 ||
					zMaxWorld != 1 || zMinWorld != 0) {
					exportReduced(meshes, savePath, box, false);
				}
				else {
					exportMeshes(meshes, savePath, box, false);
				}
			}
		}
		if (ImGui::Button("Export stats") && !meshes.empty()) {
			std::string savePath = saveFileTxt();

			if (!savePath.empty()) {
				if (xMaxWorld != 1 || xMinWorld != 0 ||
					yMaxWorld != 1 || yMinWorld != 0 ||
					zMaxWorld != 1 || zMinWorld != 0) {
					exportReduced(meshes, savePath, box, true);
				}
				else {
					exportMeshes(meshes, savePath, box, true);
				}
			}
		}

		ImGui::End();



		ImGui::SetNextWindowPos(ImVec2(0, 780.0f / 1080.0f * display.y), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(display.x, 300.0f / 1080.0f * display.y), ImGuiCond_Once);
		ImGui::Begin("Bounding box size");
		ImGui::SliderFloat("X size", &xMaxWorld, 0.0f, 1.0f);
		ImGui::SliderFloat("X offset", &xMinWorld, 0.0f, 1.0f);
		ImGui::SliderFloat("Y size", &yMaxWorld, 0.0f, 1.0f);
		ImGui::SliderFloat("Y offset", &yMinWorld, 0.0f, 1.0f);
		ImGui::SliderFloat("Z size", &zMaxWorld, 0.0f, 1.0f);
		ImGui::SliderFloat("Z offset", &zMinWorld, 0.0f, 1.0f);


		ImGui::End();

		ImGui::Begin("Player paths");
		if (ImGui::Button("Load players")) {
			std::string selectedPath = openFile();

			if (!selectedPath.empty()) {
				std::string path = selectedPath;
				players = importPlayers(path, box);
			}
			if (!players.empty()) {
				playerPaths = true;
			}
		}
		ImGui::End();

		box.Xminus = box.lim_Xminus + xMinWorld * (box.lim_Xplus - box.lim_Xminus);
		box.Yminus = box.lim_Yminus + yMinWorld * (box.lim_Yplus - box.lim_Yminus);
		box.Zminus = box.lim_Zminus + zMinWorld * (box.lim_Zplus - box.lim_Zminus);

		box.Xplus = box.Xminus + xMaxWorld * (box.lim_Xplus - box.lim_Xminus);
		box.Yplus = box.Yminus + yMaxWorld * (box.lim_Yplus - box.lim_Yminus);
		box.Zplus = box.Zminus + zMaxWorld * (box.lim_Zplus - box.lim_Zminus);

		glm::vec3 direction;
		direction.x = cos(glm::radians(pitch)) * sin(glm::radians(yaw));
		direction.y = sin(glm::radians(pitch));
		direction.z = cos(glm::radians(pitch)) * cos(glm::radians(yaw));

		glm::vec3 worldUp = glm::vec3(0, 1, 0);
		glm::vec3 right = glm::normalize(glm::cross(direction, worldUp));
		glm::vec3 up = glm::normalize(glm::cross(right, direction));

		if (!ImGui::GetIO().WantCaptureMouse &&
			ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {

			ImVec2 d = ImGui::GetIO().MouseDelta;
			yaw -= d.x * 0.5f;
			pitch -= d.y * 0.5f;

			pitch = glm::clamp(pitch, -89.0f, 89.0f);
		}

		distance -= ImGui::GetIO().MouseWheel*(zoomSpeed/5);
		distance = glm::clamp(distance, 1.0f, 1000.0f);

		if (!ImGui::GetIO().WantCaptureMouse &&
			ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {

			ImVec2 d = ImGui::GetIO().MouseDelta;

			float panSpeed = distance * moveSpeed/1000;

			target -= right * d.x * panSpeed;
			target += up * d.y * panSpeed;
		}


		ImGui::Render();

		int w, h;
		glfwGetFramebufferSize(window, &w, &h);
		glViewport(0, 0, w, h);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(0.2f, 0.2f, 0.1f, 1.0f);
		if (!meshes.empty()) {
			glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);


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
		if (!players.empty()) {
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

				glBindVertexArray(p.VAO);
				glDrawArrays(GL_LINE_STRIP, 0, p.positions.size());
				glBindVertexArray(0);
			}
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

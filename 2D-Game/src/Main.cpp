#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include <vector>
#include <array>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Render/Shader.hpp"
#include "Render/Texture.hpp"

int Screen_width = 1920;
int Screen_Height = 1080;

unsigned int VBO, VAO, EBO;
GLuint SSBO;

float Zoom = 500.0f;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float playerSpeed = 100.0f;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

extern "C" {
	__declspec(dllexport) uint32_t NvOptimusEnablement = 1;
}

struct Transform
{
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 rotation = glm::vec3(0.0f);
	glm::vec3 scale = glm::vec3(1.0f);
	glm::mat4 to_mat4() const
	{
		glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
		m *= glm::mat4_cast(glm::quat(rotation));
		m = glm::scale(m, scale);
		return m;
	}
};
Transform transform;

struct Vertex
{
	glm::vec2 position;
	glm::vec2 texCoords;
};

std::vector<Vertex> vertices;
std::vector<glm::mat4> transforms;

void CreateQuad(const Transform& t, float width, float height, float Sprite_Width, float Sprite_height)
{
	float x = 2, y = 4;
	float sheet_Width = 260.0f, sheet_height = 261.0f;

	Vertex v0;
	Vertex v1;
	Vertex v2;
	Vertex v3;
	v0.position = glm::vec2(-0.5f * width, -0.5f * height);
	v1.position = glm::vec2(0.5f * width, -0.5f * height);
	v2.position = glm::vec2(0.5f * width, 0.5f * height);
	v3.position = glm::vec2(-0.5f * width, 0.5f * height);
	v0.texCoords = glm::vec2((x + 0.f) * (Sprite_Width / sheet_Width), (y + 1.f) * (Sprite_height / sheet_height));
	v1.texCoords = glm::vec2((x + 1.f) * (Sprite_Width / sheet_Width), (y + 1.f) * (Sprite_height / sheet_height));
	v2.texCoords = glm::vec2((x + 1.f) * (Sprite_Width / sheet_Width), (y + 0.f) * (Sprite_height / sheet_height));
	v3.texCoords = glm::vec2((x + 0.f) * (Sprite_Width / sheet_Width), (y + 0.f) * (Sprite_height / sheet_height));
	vertices.push_back(v0);
	vertices.push_back(v1);
	vertices.push_back(v3);
	vertices.push_back(v1);
	vertices.push_back(v2);
	vertices.push_back(v3);

	transforms.push_back(t.to_mat4());

	glGenBuffers(1, &VBO);
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), NULL, GL_DYNAMIC_DRAW);

	// position attribute
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 1 * sizeof(Vertex), (void*)offsetof(Vertex, position));
	glEnableVertexAttribArray(0);

	// texture coord attribute
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 1 * sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
	glEnableVertexAttribArray(1);
}

int main() {

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(Screen_width, Screen_Height, "2D-Game", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "GLFW Context is incorrect...You should fix it" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "GLAD isn't initalized properly" << std::endl;
		return -1;
	}

	Shader shader("Resource/Shaders/Main-Shader.vert", "Resource/Shaders/Main-Shader.frag");
	stbi_set_flip_vertically_on_load(false);
	Texture image("Resource/Textures/spritesheet.jpg");

	Transform t;
	CreateQuad(t, 1.0f, 1.0f, 65.0f, 65.0f);

	glGenBuffers(1, &SSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, vertices.size() * sizeof(Vertex), NULL, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, SSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	while (!glfwWindowShouldClose(window))
	{		
		//Inputs
		processInput(window);

		//Render
		glClearColor(0.1f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		shader.use();

		float left = -Screen_width / (2.0f * Zoom);
		float right = Screen_width / (2.0f * Zoom);
		float bottom = -Screen_Height / (2.0f * Zoom);
		float top = Screen_Height / (2.0f * Zoom);

		glm::mat4 view = glm::mat4(1.0f);
		glm::mat4 projection = glm::mat4(1.0f);

		projection = glm::ortho(left, right, bottom, top, -0.1f, 100.0f);
		view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f));

		shader.setMat4("projection", projection);
		shader.setMat4("view", view);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER, vertices.size() * sizeof(Vertex), NULL, GL_DYNAMIC_DRAW);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, transforms.size() * sizeof(glm::mat4), transforms.data());
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), NULL, GL_DYNAMIC_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(Vertex), vertices.data());
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		image.bind(0);

		glBindVertexArray(VAO);
		glDrawArrays(GL_TRIANGLES, 0, vertices.size());

		glfwPollEvents();
		glfwSwapBuffers(window);
		
	}

	glfwTerminate();
	return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	Screen_width = width;
	Screen_Height = height;
}

void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		transform.position.x += 10.0f;
		std::cout << transform.position.x << " \n";
	}
}


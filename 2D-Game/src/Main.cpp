#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include <vector>
#include <array>
#include <cmath>
#include <filesystem>
#include <map>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Render/Shader.hpp"
#include "Render/Texture.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H  

int Screen_width = 1920;
int Screen_Height = 1080;
const unsigned int ARRAY_LIMIT = 400;

unsigned int VBO, VAO, EBO;
unsigned int TVAO, TVBO;
GLuint SSBO;

float Zoom = 500.0f;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float playerSpeed = 10.0f;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void RenderText(Shader& shader, std::string text, float x, float y, float scale, glm::vec3 color);
void TextRenderCall(int length, GLuint shader);
void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length, const char* message, const void* userParam);

extern "C" {
	__declspec(dllexport) uint32_t NvOptimusEnablement = 1;
}

struct Character {
	int TextureID; // ID handle of the glyph texture
	glm::ivec2   Size;      // Size of glyph
	glm::ivec2   Bearing;   // Offset from baseline to left/top of glyph
	unsigned int Advance;   // Horizontal offset to advance to next glyph
};
std::map<GLchar, Character> Characters;
GLuint textureArray;
std::vector<int>letterMap;

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
std::vector<glm::mat4>T;

void CreateQuad(const Transform& t, float width, float height, float Sprite_Width, float Sprite_height)
{
	float x = 3, y = 4;
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
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);


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

	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(glDebugOutput, nullptr);

	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	Shader shader("Resource/Shaders/Main-Shader.vert", "Resource/Shaders/Main-Shader.frag");
	Shader Text_Render("Resource/Shaders/Text-Render.vert", "Resource/Shaders/Text-Render.frag");
	stbi_set_flip_vertically_on_load(false);
	Texture image("Resource/Textures/spritesheet.jpg");

	FT_Library ft;
	// All functions return a value different than 0 whenever an error occurred
	if (FT_Init_FreeType(&ft))
	{
		std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
		return -1;
	}

	// find path to font
	std::string font_name = std::string("Resource/Fonts/PressStart2P-Regular.ttf");
	if (font_name.empty())
	{
		std::cout << "ERROR::FREETYPE: Failed to load font_name" << std::endl;
		return -1;
	}

	// load font as face
	FT_Face face;
	if (FT_New_Face(ft, font_name.c_str(), 0, &face)) {
		std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
		return -1;
	}
	else {
		// set size to load glyphs as
		FT_Set_Pixel_Sizes(face, 256, 256);

		// disable byte-alignment restriction
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glGenTextures(1, &textureArray);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R8, 256, 256, 128, 0, GL_RED, GL_UNSIGNED_BYTE, 0);

		// load first 128 characters of ASCII set
		for (unsigned char c = 0; c < 128; c++)
		{
			// Load character glyph 
			if (FT_Load_Char(face, c, FT_LOAD_RENDER))
			{
				std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
				continue;
			}
			glTexSubImage3D(
				GL_TEXTURE_2D_ARRAY,
				0, 0, 0, int(c),
				face->glyph->bitmap.width,
				face->glyph->bitmap.rows, 1,
				GL_RED,
				GL_UNSIGNED_BYTE,
				face->glyph->bitmap.buffer
			);
			// set texture options
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			// now store character for later use
			Character character = {
				int(c),
				glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
				glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
				static_cast<unsigned int>(face->glyph->advance.x)
			};
			Characters.insert(std::pair<char, Character>(c, character));
		}
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}
	// destroy FreeType once we're finished
	FT_Done_Face(face);
	FT_Done_FreeType(ft);

	for (int i = 0; i < ARRAY_LIMIT; i++) {
		letterMap.push_back(0);
		T.push_back(glm::mat4(1.0f));
	}

	GLfloat vertex_data[] = {
	0.0f,1.0f,
	0.0f,0.0f,
	1.0f,1.0f,
	1.0f,0.0f,
	};

	glGenVertexArrays(1, &TVAO);
	glGenBuffers(1, &TVBO);
	glBindVertexArray(TVAO);

	glBindBuffer(GL_ARRAY_BUFFER, TVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glGenBuffers(1, &SSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, vertices.size() * sizeof(Vertex), NULL, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, SSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	Transform t;
	CreateQuad(t, 1.0f, 1.0f, 65.0f, 65.0f);

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

		Text_Render.use();
		Text_Render.setMat4("projection", projection);
		RenderText(Text_Render, "Hello There", 0.0f, 0.0f, 10.0f, glm::vec3(0.2, 0.5f, 0.6f));

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

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		transform.position.y += playerSpeed * deltaTime;
	}
		

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		transform.position.y -= playerSpeed * deltaTime;
	}
		

	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		transform.position.x -= playerSpeed * deltaTime;
	}
		

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		transform.position.x += playerSpeed * deltaTime;
	}
		
	transforms[0] = transform.to_mat4();
}

void RenderText(Shader& shader, std::string text, float x, float y, float scale, glm::vec3 color)
{
	scale = scale * 48.0f / 256.0f;
	float copyX = x;
	// activate corresponding render state	
	shader.use();
	glUniform3f(glGetUniformLocation(shader.ID, "textColor"), color.x, color.y, color.z);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);
	glBindVertexArray(TVAO);
	//glBindBuffer(GL_ARRAY_BUFFER, TVBO);

	int workingIndex = 0;
	// iterate through all characters
	std::string::const_iterator c;
	for (c = text.begin(); c != text.end(); c++)
	{

		Character ch = Characters[*c];

		if (*c == '\n') {
			y -= ((ch.Size.y)) * 1.3 * scale;
			x = copyX;
		}
		else if (*c == ' ') {
			x += (ch.Advance >> 6) * scale;
		}
		else
		{
			float xpos = x + ch.Bearing.x * scale;
			float ypos = y - (256 - ch.Bearing.y) * scale;

			//T[workingIndex] = glm::translate(glm::mat4(1.0f), glm::vec3(xpos, ypos, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(256 * scale, 256 * scale, 0));
			T[workingIndex] = glm::translate(glm::mat4(1.0f), glm::vec3(xpos, ypos, 0));
			letterMap[workingIndex] = ch.TextureID;

			// now advance cursors for next glyph (note that advance is number of 1/64 pixels)
			x += (ch.Advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
			workingIndex++;
			if (workingIndex == ARRAY_LIMIT - 1) {
				TextRenderCall(workingIndex, shader.ID);
				workingIndex = 0;
			}
		}


	}
	TextRenderCall(workingIndex, shader.ID);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

}

void TextRenderCall(int length, GLuint shader)
{
	if (length != 0) {
		glUniformMatrix4fv(glGetUniformLocation(shader, "transforms"), length, GL_FALSE, &T[0][0][0]);
		glUniform1iv(glGetUniformLocation(shader, "letterMap"), length, &letterMap[0]);
		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, length);
	}

}

void glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length, const char* message, const void* userParam)
{
	// ignore non-significant error/warning codes
	if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

	std::cout << "---------------" << std::endl;
	std::cout << "Debug message (" << id << "): " << message << std::endl;


	switch (source)
	{
	case GL_DEBUG_SOURCE_API:             std::cout << "Source: API"; break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   std::cout << "Source: Window System"; break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cout << "Source: Shader Compiler"; break;
	case GL_DEBUG_SOURCE_THIRD_PARTY:     std::cout << "Source: Third Party"; break;
	case GL_DEBUG_SOURCE_APPLICATION:     std::cout << "Source: Application"; break;
	case GL_DEBUG_SOURCE_OTHER:           std::cout << "Source: Other"; break;
	} std::cout << std::endl;

	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR:               std::cout << "Type: Error"; break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cout << "Type: Deprecated Behaviour"; break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  std::cout << "Type: Undefined Behaviour"; break;
	case GL_DEBUG_TYPE_PORTABILITY:         std::cout << "Type: Portability"; break;
	case GL_DEBUG_TYPE_PERFORMANCE:         std::cout << "Type: Performance"; break;
	case GL_DEBUG_TYPE_MARKER:              std::cout << "Type: Marker"; break;
	case GL_DEBUG_TYPE_PUSH_GROUP:          std::cout << "Type: Push Group"; break;
	case GL_DEBUG_TYPE_POP_GROUP:           std::cout << "Type: Pop Group"; break;
	case GL_DEBUG_TYPE_OTHER:               std::cout << "Type: Other"; break;
	} std::cout << std::endl;

	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH:         std::cout << "Severity: high"; break;
	case GL_DEBUG_SEVERITY_MEDIUM:       std::cout << "Severity: medium"; break;
	case GL_DEBUG_SEVERITY_LOW:          std::cout << "Severity: low"; break;
	case GL_DEBUG_SEVERITY_NOTIFICATION: std::cout << "Severity: notification"; break;
	} std::cout << std::endl;
	std::cout << std::endl;

}




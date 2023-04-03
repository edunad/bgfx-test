#ifdef _WIN32
	#include <windows.h>
#endif

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#include <GLFW/glfw3.h>

#if _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3native.h>

#include <fmt/printf.h>
#include <magic_enum.hpp>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <iostream>
#include <cstdio>

struct PosColorVertex
{
    float x;
    float y;
    float z;
    uint32_t abgr;
};

static PosColorVertex cubeVertices[] =
{
    {-1.0f,  1.0f,  1.0f, 0xff000000 },
    { 1.0f,  1.0f,  1.0f, 0xff0000ff },
    {-1.0f, -1.0f,  1.0f, 0xff00ff00 },
    { 1.0f, -1.0f,  1.0f, 0xff00ffff },
    {-1.0f,  1.0f, -1.0f, 0xffff0000 },
    { 1.0f,  1.0f, -1.0f, 0xffff00ff },
    {-1.0f, -1.0f, -1.0f, 0xffffff00 },
    { 1.0f, -1.0f, -1.0f, 0xffffffff },
};

static const uint16_t cubeTriList[] =
{
    0, 1, 2,
    1, 3, 2,
    4, 6, 5,
    5, 6, 7,
    0, 2, 4,
    4, 2, 6,
    1, 5, 3,
    5, 7, 3,
    0, 4, 1,
    4, 5, 1,
    2, 3, 6,
    6, 3, 7,
};

static void glfw_errorCallback(int error, const char *description) {
	fmt::print("GLFW error {}: {}\n", error, description);
}

bgfx::ShaderHandle loadShader(const std::string& fileName) {
    FILE *file = fopen(fmt::format("./content/shaders/{}", fileName).c_str(), "rb");

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    const bgfx::Memory *mem = bgfx::alloc(fileSize + 1);
    fread(mem->data, 1, fileSize, file);
    mem->data[mem->size - 1] = '\0';
    fclose(file);

    return bgfx::createShader(mem);
}

int main(int argc, char* argv[]) {
	int width = 1024;
	int height = 768;

	#ifdef _WIN32
		SetConsoleTitle("BGFX Test");
	#endif

	glfwSetErrorCallback(glfw_errorCallback);
	if (!glfwInit())
		return 1;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Disable opengl
	GLFWwindow *window = glfwCreateWindow(width, height, "test", nullptr, nullptr);
	if (!window) return 1;

	// Call bgfx::renderFrame before bgfx::init to signal to bgfx not to create a render thread.
	// Most graphics APIs must be used on the same thread that created the window.
	bgfx::renderFrame();
	bgfx::Init init;
	init.type = bgfx::RendererType::Vulkan;
	init.resolution.width = (uint32_t)width;
	init.resolution.height = (uint32_t)height;
	init.resolution.reset = BGFX_RESET_VSYNC;
	#if _WIN32
		init.platformData.nwh = glfwGetWin32Window(window);
	#else
		init.platformData.ndt = glfwGetX11Display();
		init.platformData.nwh = (void*)(uintptr_t)glfwGetX11Window(window);
	#endif

	if (!bgfx::init(init)) return 1;

	bgfx::setDebug(BGFX_DEBUG_STATS); // Enable debug.

	// Set view 0 to the same dimensions as the window and to clear the color buffer.
	const bgfx::ViewId kClearView = 0;
	bgfx::setViewClear(kClearView, BGFX_CLEAR_COLOR);
	bgfx::setViewRect(kClearView, 0, 0, bgfx::BackbufferRatio::Equal);

	// Setup
	bgfx::VertexLayout pcvDecl;
    pcvDecl.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
    .end();

	bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(bgfx::makeRef(cubeVertices, sizeof(cubeVertices)), pcvDecl);
    bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(bgfx::makeRef(cubeTriList, sizeof(cubeTriList)));

	// Load shaders
	bgfx::ShaderHandle vsh = loadShader("vs_cubes.bin");
    bgfx::ShaderHandle fsh = loadShader("fs_cubes.bin");
    bgfx::ProgramHandle program = bgfx::createProgram(vsh, fsh, true);

	// Setup camera
	glm::vec3 position = {1.0f, 1.0f, -2.0f};
	glm::vec3 target = {0.0f, 0.0f, 0.0f};
	glm::vec3 up = {0.f, 1.f, 0.f};

	auto view = glm::lookAt(position, target, up);
	auto proj = glm::perspective(glm::radians(60.f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);

	float i = 0;

	// Start program
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		// Handle window resize.
		int oldWidth = width, oldHeight = height;
		glfwGetWindowSize(window, &width, &height);
		if (width != oldWidth || height != oldHeight) {
			bgfx::reset((uint32_t)width, (uint32_t)height, BGFX_RESET_VSYNC);
			bgfx::setViewRect(kClearView, 0, 0, bgfx::BackbufferRatio::Equal);
		}

		// This dummy draw call is here to make sure that view 0 is cleared if no other draw calls are submitted to view 0.
		bgfx::touch(kClearView);

		// DRAW
		bgfx::setViewTransform(0, glm::value_ptr(view), glm::value_ptr(proj));

        bgfx::setVertexBuffer(0, vbh);
        bgfx::setIndexBuffer(ibh);

		// ROTATE CUBEEEE
		/*float mtx[16];
		bx::mtxRotateXY(mtx, i * 0.01f, i * 0.01f);
		bgfx::setTransform(mtx);*/

		// Submit shader
        bgfx::submit(0, program);

		// Advance to next frame. Process submitted rendering primitives.
		bgfx::frame();
		i++;
	}

	bgfx::shutdown();
	glfwTerminate();

	return 0;
}

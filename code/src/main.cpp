#ifdef _WIN32
	#include <windows.h>
#endif

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bgfx/embedded_shader.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if GLFW_VERSION_MINOR < 2
#	error "GLFW 3.2 or later is required"
#endif // GLFW_VERSION_MINOR < 2

#if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
#	if USE_WAYLAND
#		include <wayland-egl.h>
#		define GLFW_EXPOSE_NATIVE_WAYLAND
#	else
#		define GLFW_EXPOSE_NATIVE_X11
#		define GLFW_EXPOSE_NATIVE_GLX
#	endif
#elif BX_PLATFORM_OSX
#	define GLFW_EXPOSE_NATIVE_COCOA
#	define GLFW_EXPOSE_NATIVE_NSGL
#elif BX_PLATFORM_WINDOWS
#	define GLFW_EXPOSE_NATIVE_WIN32
#	define GLFW_EXPOSE_NATIVE_WGL
#endif //
#include <GLFW/glfw3native.h>

#include <bx/math.h>
#include <fmt/printf.h>

#include <stdexcept>

// Compiled shaders
#include <generated/shaders/bgfx-test/all.h>

#if _WIN32
	static const bgfx::EmbeddedShader s_embeddedShaders[] =
	{
		BGFX_EMBEDDED_SHADER(vs_cubes),
		BGFX_EMBEDDED_SHADER(fs_cubes),
		BGFX_EMBEDDED_SHADER_END()
	};
#else
	#define BGFX_EMBEDDED_SHADER_VULKAN(_name)                                                 \
		{                                                                                      \
			#_name,                                                                            \
			{                                                                                  \
				BGFX_EMBEDDED_SHADER_SPIRV(bgfx::RendererType::Vulkan,     _name)              \
				{ bgfx::RendererType::Noop,  (const uint8_t*)"VSH\x5\x0\x0\x0\x0\x0\x0", 10 }, \
				{ bgfx::RendererType::Count, NULL, 0 }                                         \
			}                                                                                  \
		}

	static const bgfx::EmbeddedShader s_embeddedShaders[] =
	{
		BGFX_EMBEDDED_SHADER_VULKAN(vs_cubes),
		BGFX_EMBEDDED_SHADER_VULKAN(fs_cubes),
		BGFX_EMBEDDED_SHADER_END()
	};
#endif


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

static void* glfwNativeWindowHandle(GLFWwindow* _window) {
#	if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
# 		if defined(GLFW_EXPOSE_NATIVE_WAYLAND)
			wl_egl_window *win_impl = (wl_egl_window*)glfwGetWindowUserPointer(_window);
			if(!win_impl)
			{
				int width, height;
				glfwGetWindowSize(_window, &width, &height);
				struct wl_surface* surface = (struct wl_surface*)glfwGetWaylandWindow(_window);
				if(!surface)
					return nullptr;
				win_impl = wl_egl_window_create(surface, width, height);
				glfwSetWindowUserPointer(_window, (void*)(uintptr_t)win_impl);
			}
			return (void*)(uintptr_t)win_impl;
#		else
			return (void*)(uintptr_t)glfwGetX11Window(_window);
#		endif
#	elif BX_PLATFORM_OSX
		return glfwGetCocoaWindow(_window);
#	elif BX_PLATFORM_WINDOWS
		return glfwGetWin32Window(_window);
#	endif // BX_PLATFORM_
}

static void glfwDestroyWindowImpl(GLFWwindow *_window){
	if(!_window) return;

	#if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
		#if defined(GLFW_EXPOSE_NATIVE_WAYLAND)
			wl_egl_window *win_impl = (wl_egl_window*)glfwGetWindowUserPointer(_window);
			if(win_impl)
			{
				glfwSetWindowUserPointer(_window, nullptr);
				wl_egl_window_destroy(win_impl);
			}
		#endif
	#endif

	glfwDestroyWindow(_window);
}

void* getNativeDisplayHandle() {
#	if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
#		if defined(GLFW_EXPOSE_NATIVE_WAYLAND)
			return glfwGetWaylandDisplay();
#		else
			return glfwGetX11Display();
#		endif
#	else
		return NULL;
#	endif // BX_PLATFORM_*
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

	bgfx::Init init;
	init.type = bgfx::RendererType::Count;
	init.resolution.width = (uint32_t)width;
	init.resolution.height = (uint32_t)height;
	init.resolution.reset = BGFX_RESET_VSYNC;
	init.platformData.nwh = glfwNativeWindowHandle(window);
	init.platformData.ndt = getNativeDisplayHandle();


	// Call bgfx::renderFrame before bgfx::init to signal to bgfx not to create a render thread.
	// Most graphics APIs must be used on the same thread that created the window.
	bgfx::renderFrame();
	if (!bgfx::init(init)) return 1;

	bgfx::setDebug(BGFX_DEBUG_STATS); // Enable debug.

	// Set view 0 to the same dimensions as the window and to clear the color buffer.
	const bgfx::ViewId kClearView = 0;
	bgfx::setViewClear(kClearView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x443355FF, 1.0f, 0);
	bgfx::setViewRect(kClearView, 0, 0, bgfx::BackbufferRatio::Equal);

	// Setup camera
	float view[16];
	bx::mtxLookAt(view, {0.0f, 0.0f, -5.0f}, {0.0f, 0.0f,  0.0f}); // Am too lazy to fix it for glm

    float proj[16];
	bx::mtxProj(proj, 60.0f, static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);

	bgfx::setViewTransform(0, view, proj);

	// Setup
	bgfx::VertexLayout pcvDecl;
    pcvDecl.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
    .end();

	bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(bgfx::makeRef(cubeVertices, sizeof(cubeVertices)), pcvDecl);
    bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(bgfx::makeRef(cubeTriList, sizeof(cubeTriList)));

	// Load shaders
	bgfx::RendererType::Enum type = bgfx::getRendererType();
	bgfx::ShaderHandle vsh = bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_cubes");
	bgfx::ShaderHandle fsh = bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_cubes");
    bgfx::ProgramHandle program = bgfx::createProgram(vsh, fsh, true);
	if(program.idx == 0) throw std::runtime_error("Failed to initialize shader program");

	float i = 0;
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
        bgfx::setVertexBuffer(0, vbh);
        bgfx::setIndexBuffer(ibh);

		// ROTATE CUBEEEE
		float mtx[16];
		bx::mtxRotateXY(mtx, i * 0.01f, i * 0.01f);
		bgfx::setTransform(mtx);

		// Submit shader
        bgfx::submit(0, program);

		// Advance to next frame. Process submitted rendering primitives.
		bgfx::frame();
		i++;
	}

	bgfx::shutdown();

	//glfwDestroyWindowImpl(window);
	glfwTerminate();

	return 0;
}

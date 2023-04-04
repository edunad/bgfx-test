from conan import ConanFile

class Recipe(ConanFile):
	settings = "os", "compiler", "build_type", "arch"
	requires = "fmt/9.1.0", "glm/cci.20230113"
	generators = "CMakeDeps", "CMakeToolchain"

	def requirements(self):
		if self.settings.os == "Linux":
			self.requires("wayland/1.21.0")


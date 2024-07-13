from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps


class libsosimpleRecipe(ConanFile):
    name = "sosimple"
    version = "1.0"

    # Optional metadata
    license = "MIT License"
    author = "DosMike"
    url = "https://github.com/DosMike/libsosimple"
    description = "Easy to use socket library"
    topics = ("network", "tcp", "udp", "unicast", "multicast", "socket")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "sosimple.pc.in", "CMakeLists.txt", "src/*", "include/*"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.24]")

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        if self.options.shared:
            self.cpp_info.libs = ["sosimple-shared"]
        else:
            self.cpp_info.libs = ["sosimple-static"]

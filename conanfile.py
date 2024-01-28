from conan import ConanFile


class Recipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps", "VirtualRunEnv"

    def layout(self):
        self.folders.generators = "conan"

    def requirements(self):
        self.requires("boost/[>=1.70.0]")
        # fixes compiler warning for MSVC and c++20Relea
        self.requires("fmt/[>=10.1.0]")

        # Testing only dependencies below
        self.requires("catch2/3.0.1")
        self.requires("benchmark/1.8.3")

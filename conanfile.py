from conan import ConanFile


class Recipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps", "VirtualRunEnv"

    def layout(self):
        self.folders.generators = "conan"

    def requirements(self):
        self.requires("boost/[>=1.85.0]", override=True)
        # fixes compiler warning for MSVC and c++20 Release
        self.requires("fmt/[>=10.1.0]")
        self.requires("msgpack-cxx/[>=6.0.0]")

        # Testing only dependencies below
        self.requires("gtest/[>1 <2]")
        self.requires("benchmark/[>1 <2]")

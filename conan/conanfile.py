from conan.tools.microsoft import msvc_runtime_flag
from conans import ConanFile, CMake, tools
from conans.errors import ConanInvalidConfiguration
import os
import functools

required_conan_version = ">=1.43.0"


class SoralogConan(ConanFile):
    name = "soralog"
    description = "Google's C++ test framework"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://github.com/xdimon/soralog"
    license = "Apache-2.0"
    topics = ("logging")

    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    exports_sources = "CMakeLists.txt"
    generators = "cmake", "cmake_find_package", "cmake_find_package_multi"
    _cmake = None

    @property
    def _source_subfolder(self):
        return "source_subfolder"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            del self.options.fPIC

    def requirements(self):
        self.requires("fmt/8.1.1")
        self.requires("yaml-cpp/0.7.0")

    def source(self):
        tools.get(**self.conan_data["sources"][self.version],
                  destination=self._source_subfolder, strip_root=True)

    def _configure_cmake(self):
        if self._cmake:
            return self._cmake
        self._cmake = CMake(self)
        self._cmake.definitions["EXAMPLES"] = False
        self._cmake.definitions["PACMAN"] = "CONAN"
        self._cmake.definitions["TESTING"] = False
        self._cmake.configure()
        return self._cmake

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()

    def package(self):
        self.copy("LICENSE", dst="licenses", src=self._source_subfolder)
        cmake = self._configure_cmake()
        cmake.install()
        # tools.rmdir(os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        target = "soralog"
        self.cpp_info.set_property("cmake_file_name", "soralog")
        self.cpp_info.set_property("cmake_target_name", "soralog::{}".format(target))
        self.cpp_info.set_property("pkg_config_name", "soralog")

        self.cpp_info.names["cmake_find_package"] = "soralog"
        self.cpp_info.names["cmake_find_package_multi"] = "soralog"
        self.cpp_info.components["libsoralog"].names["cmake_find_package"] = target
        self.cpp_info.components["libsoralog"].names["cmake_find_package_multi"] = target

        self.cpp_info.components["libsoralog"].requires = ["fmt::fmt", "yaml-cpp::yaml-cpp"]
        self.cpp_info.components["libsoralog"].libs = tools.collect_libs(self)

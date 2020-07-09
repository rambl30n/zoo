import nlo

class Zoo(nlo.NloBasicCmakeConanFile):
    name = "zoo"
    generators = ("nlo_cmake_link", "cmake", "json")
    exports_sources = ["CMakeLists.txt", "inc/*"]

    exports_sources_git = {
        "branch": "auto",
        "url": "auto",
        "commit": "auto",
        "conanfile_rel_path": "auto"
    }

    options = {
        "enable_testing": {True, False},
    }

    default_options = {
        "enable_testing": False,
    }

    def build(self):
        self.simple_cmake_build()

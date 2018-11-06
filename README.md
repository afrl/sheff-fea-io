# fea_io

A templated parallel I/O library for storing FEA meshes, data, coordinate systems, etc.

Instructions:
- git clone https://github.com/nlohmann/json.git
- CMake generate, configure, and open
- Install in VS
- git clone this repo
- set nlohmann_json_DIR (configure will bring it up) to the directory that you set as the build directory of the JSON library
- Set install prefix
- Configure, generate, and open
- In VS, you can just install to build the project to the install location (since it is header only)
- In depending projects, you can just include the directory, or you can use find_package
#include "CityGenerator.h"
#include "Config.h"

#include <iostream>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

/**
 * @brief Parse a command-line argument of the form --key=value.
 *
 * If the argument starts with the given prefix ("--key="), the substring
 * following the prefix is returned.  Otherwise an empty string is returned.
 */
static std::string parseArg(const std::string &arg, const std::string &prefix) {
    if (arg.rfind(prefix, 0) == 0) { // starts with prefix
        return arg.substr(prefix.size());
    }
    return std::string();
}

/**
 * @brief Entry point for the command-line city generator.
 *
 * Usage:
 *   citygen --population=100000 --hospitals=2 --schools=3 \
 *           --transport=car --seed=42 --grid-size=100 \
 *           --radius-fraction=0.8 --output=out_dir
 *
 * The program will produce a OBJ file (city.obj) and a summary JSON
 * (city_summary.json) in the specified output directory.
 */
int main(int argc, char **argv) {
    Config cfg;
    std::string outDir;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (auto s = parseArg(arg, "--population="); !s.empty()) {
            cfg.population = static_cast<std::uint32_t>(std::strtoul(s.c_str(), nullptr, 10));
        } else if (auto s = parseArg(arg, "--hospitals="); !s.empty()) {
            cfg.hospitals = static_cast<std::uint32_t>(std::strtoul(s.c_str(), nullptr, 10));
        } else if (auto s = parseArg(arg, "--schools="); !s.empty()) {
            cfg.schools = static_cast<std::uint32_t>(std::strtoul(s.c_str(), nullptr, 10));
        } else if (auto s = parseArg(arg, "--transport="); !s.empty()) {
            cfg.transport_mode = transportModeFromString(s);
        } else if (auto s = parseArg(arg, "--seed="); !s.empty()) {
            cfg.seed = static_cast<std::uint32_t>(std::strtoul(s.c_str(), nullptr, 10));
        } else if (auto s = parseArg(arg, "--grid-size="); !s.empty()) {
            cfg.grid_size = static_cast<int>(std::strtol(s.c_str(), nullptr, 10));
        } else if (auto s = parseArg(arg, "--radius-fraction="); !s.empty()) {
            cfg.city_radius = std::strtod(s.c_str(), nullptr);
        } else if (auto s = parseArg(arg, "--format="); !s.empty()) {
            try {
                cfg.export_format = exportFormatFromString(s);
            } catch (const std::invalid_argument &e) {
                std::cerr << e.what() << std::endl;
                return 1;
            }
        } else if (auto s = parseArg(arg, "--output="); !s.empty()) {
            outDir = s;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: citygen [options]\n\n"
                      << "Options:\n"
                      << "  --population=<number>      Number of inhabitants (default 100000)\n"
                      << "  --hospitals=<number>       Number of hospitals to place (default 1)\n"
                      << "  --schools=<number>         Number of schools to place (default 1)\n"
                      << "  --transport=<mode>         Primary transport mode (car|transit|walk)\n"
                      << "  --seed=<number>            RNG seed (default 0)\n"
                      << "  --grid-size=<number>       Width/height of the grid (default 100)\n"
                      << "  --radius-fraction=<float>  Fraction of half grid forming city radius (default 0.8)\n"
                      << "  --format=<obj|gltf|glb>    Output mesh format (default obj)\n"
                      << "  --output=<dir>             Directory to output results (required)\n"
                      << std::endl;
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            return 1;
        }
    }
    if (outDir.empty()) {
        std::cerr << "Error: --output=<dir> must be specified" << std::endl;
        return 1;
    }
    // Create output directory if it does not exist
    std::filesystem::create_directories(outDir);
    // Generate city
    City city = CityGenerator::generate(cfg);
    // Save outputs
    std::string objPath = outDir + "/city.obj";
    std::string gltfPath = outDir + "/city.gltf";
    std::string glbPath = outDir + "/city.glb";
    std::string modelPath;
    std::string summaryPath = outDir + "/city_summary.json";
    switch (cfg.export_format) {
        case Config::ExportFormat::OBJ:
            city.saveOBJ(objPath);
            modelPath = objPath;
            break;
        case Config::ExportFormat::GLB:
            city.saveGLTF(glbPath, true);
            modelPath = glbPath;
            break;
        case Config::ExportFormat::GLTF:
        default:
            city.saveGLTF(gltfPath, false);
            modelPath = gltfPath;
            break;
    }
    city.saveSummary(summaryPath);
    std::cout << "Generated city at: " << modelPath << " and summary: " << summaryPath << std::endl;
    return 0;
}

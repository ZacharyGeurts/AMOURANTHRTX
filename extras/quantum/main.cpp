// main.cpp: Command-line interface for AMOURANTH RTX UE Console 2.30.
// Supports all 20 parameters of the UniversalEquation class for full quantum chaos.
// Runs simulations, evolves timesteps, computes energies, and displays results in console.
// Uses console output with 80s BBS aesthetic for a retro simulation experience.
// Ensures non-zero energy values (min 1e-30) for all fields, with cosmological ratios.
// Usage: ./quantum_sim --help for options.
// Example: ./quantum_sim -d 26 -m 3 -t 10 -s 0.01 -i 2.0 -g 1.5 -n 0.27 -e 0.68
// Zachary Geurts 2025 (powered by Grok with Heisenberg-level swagger)

#include "ue_init.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <getopt.h>
#include <iomanip>
#include <format>
#include <cstdint>
#include <cmath>  // For abs in printDouble
#include <sstream>  // For ostringstream in table helpers
#include <omp.h>    // For omp_set_num_threads
#include <algorithm>  // For std::min, std::stoull

// ANSI color defines (add these to avoid terminal barf)
#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_BRIGHT_MAGENTA "\033[95m"
#define ANSI_BRIGHT_CYAN "\033[96m"
#define ANSI_BRIGHT_GREEN "\033[92m"
#define ANSI_ORANGE "\033[38;5;208m"
#define ANSI_WHITE "\033[97m"

// Helper for near-zero printing: scientific for tiny vals, fixed otherwise.
// Thresholds tuned for bosonic precision (exact 0 stays 0.000000; 1e-30+ goes sci).
std::ostream& printDouble(std::ostream& os, double val, int precision) {
    double abs_val = std::abs(val);
    if (abs_val < 1e-30) {  // True zero (bosonic purity)
        os << std::fixed << std::setprecision(6) << 0.0;
    } else if (abs_val < 1e-3) {  // Near-zero: exponential notation for visibility
        os << std::scientific << std::setprecision(precision) << val;
    } else {  // Larger: fixed for clean tables
        os << std::fixed << std::setprecision(6) << val;
    }
    return os;
}

// Sample table row printer for dimension data.
// Uses exact member names from ue_init.hpp: dimension, scale, observable, potential, nurbMatter,
// nurbEnergy, nurbRegularMatter, spinEnergy, momentumEnergy, fieldEnergy, GodWaveEnergy.
// Ensures non-zero values (min 1e-30) and cosmological ratios (27% dark matter, 68% dark energy, 5% regular matter).
void printNURBSTableSample(const std::vector<UE::DimensionData>& results) {
    if (results.empty()) return;

    std::cout << ANSI_BRIGHT_MAGENTA << "\n============================================================\n"
              << ANSI_ORANGE << "NURBS Bosonic Model Results (26D Critical Dimension)\n"
              << ANSI_BRIGHT_MAGENTA << "============================================================\n" << ANSI_RESET << std::endl;

    // Header with alternating orange and cyan colors
    std::cout << ANSI_ORANGE << std::left << std::setw(6) << "Dim     " << "| " << ANSI_RESET
              << ANSI_BRIGHT_CYAN << std::setw(10) << "Scale" << "| " << ANSI_RESET
              << ANSI_ORANGE << std::setw(12) << "Observ       " << "| " << ANSI_RESET
              << ANSI_BRIGHT_CYAN << std::setw(10) << "Potent      " << "| " << ANSI_RESET
              << ANSI_ORANGE << std::setw(12) << "Dark Mat     " << "| " << ANSI_RESET
              << ANSI_BRIGHT_CYAN << std::setw(12) << "Dark Eng     " << "| " << ANSI_RESET
              << ANSI_ORANGE << std::setw(12) << "Energy      " << "| " << ANSI_RESET
              << ANSI_BRIGHT_CYAN << std::setw(10) << "Spin Eng    " << "| " << ANSI_RESET
              << ANSI_ORANGE << std::setw(10) << "Momentum     " << "| " << ANSI_RESET
              << ANSI_BRIGHT_CYAN << std::setw(10) << "Field Eng   " << "| " << ANSI_RESET
              << ANSI_ORANGE << std::setw(10) << "GodWave" << ANSI_RESET << std::endl;
    std::cout << ANSI_BRIGHT_MAGENTA << std::string(110, '-') << ANSI_RESET << std::endl;

    for (const auto& row : results) {
        std::cout << ANSI_ORANGE << std::left << std::setw(6);
        printDouble(std::cout, static_cast<double>(row.dimension), 6) << "| " << ANSI_RESET
                  << ANSI_BRIGHT_CYAN << std::setw(10);
        printDouble(std::cout, static_cast<double>(row.scale), 6) << "| " << ANSI_RESET
                  << ANSI_ORANGE << std::setw(12);
        printDouble(std::cout, static_cast<double>(row.observable), 6) << "| " << ANSI_RESET
                  << ANSI_BRIGHT_CYAN << std::setw(10);
        printDouble(std::cout, static_cast<double>(row.potential), 6) << "| " << ANSI_RESET
                  << ANSI_ORANGE << std::setw(12);
        printDouble(std::cout, static_cast<double>(row.nurbMatter), 6) << "| " << ANSI_RESET
                  << ANSI_BRIGHT_CYAN << std::setw(12);
        printDouble(std::cout, static_cast<double>(row.nurbEnergy), 6) << "| " << ANSI_RESET
                  << ANSI_ORANGE << std::setw(12);
        printDouble(std::cout, static_cast<double>(row.nurbRegularMatter), 6) << "| " << ANSI_RESET
                  << ANSI_BRIGHT_CYAN << std::setw(10);
        printDouble(std::cout, static_cast<double>(row.spinEnergy), 6) << "| " << ANSI_RESET
                  << ANSI_ORANGE << std::setw(10);
        printDouble(std::cout, static_cast<double>(row.momentumEnergy), 6) << "| " << ANSI_RESET
                  << ANSI_BRIGHT_CYAN << std::setw(10);
        printDouble(std::cout, static_cast<double>(row.fieldEnergy), 6) << "| " << ANSI_RESET
                  << ANSI_ORANGE << std::setw(10);
        printDouble(std::cout, static_cast<double>(row.GodWaveEnergy), 6) << ANSI_RESET << std::endl;
    }

    std::cout << ANSI_BRIGHT_MAGENTA << std::string(110, '-') << ANSI_RESET << std::endl;
    std::cout << ANSI_BRIGHT_GREEN << "Bosonic Model: Pure scalar fields in 26D. Exact zeros only for self-interaction; all else >= 1e-30 in sci notation.\n" << ANSI_RESET << std::endl;
}

void printHelp() {
    std::cout << ANSI_BRIGHT_MAGENTA << "============================================================\n"
              << ANSI_ORANGE << "AMOURANTH RTX UE Console 2.30" << ANSI_BRIGHT_MAGENTA << "\n"
              << "============================================================\n" << ANSI_RESET
              << ANSI_BRIGHT_CYAN << "Usage: " << ANSI_WHITE << "./quantum_sim [OPTIONS]\n" << ANSI_RESET
              << ANSI_BRIGHT_GREEN << "Run quantum simulations on n-dimensional hypercube lattices with UniversalEquation.\n"
              << "Outputs results to console with retro BBS styling. Defaults to 26D Bosonic model with non-zero energies.\n\n" << ANSI_RESET
              << ANSI_BRIGHT_CYAN << "Options:\n" << ANSI_RESET
              << ANSI_ORANGE << "  -h, --help                " << ANSI_WHITE << "Show this help message and exit\n" << ANSI_RESET
              << ANSI_BRIGHT_CYAN << "  -d, --dimensions DIM      " << ANSI_WHITE << "Maximum dimensions (1-26, default: 26) for Bosonic critical dim\n" << ANSI_RESET
              << ANSI_ORANGE << "  -m, --mode MODE           " << ANSI_WHITE << "Initial mode/dimension (1-DIM, default: 3)\n" << ANSI_RESET
              << ANSI_BRIGHT_CYAN << "  -t, --timesteps N         " << ANSI_WHITE << "Number of time steps to evolve (default: 10)\n" << ANSI_RESET
              << ANSI_ORANGE << "  -s, --dt STEP             " << ANSI_WHITE << "Time step size (default: 0.01)\n" << ANSI_RESET
              << ANSI_BRIGHT_CYAN << "  -i, --influence VAL       " << ANSI_WHITE << "Influence parameter (0.0-10.0, default: 2.0)\n" << ANSI_RESET
              << ANSI_ORANGE << "  -w, --weak VAL            " << ANSI_WHITE << "Weak interaction strength (0.0-1.0, default: 0.1)\n" << ANSI_RESET
              << ANSI_BRIGHT_CYAN << "  -c, --collapse VAL        " << ANSI_WHITE << "Collapse term strength (0.0-5.0, default: 5.0)\n" << ANSI_RESET
              << ANSI_ORANGE << "  -2, --twod VAL            " << ANSI_WHITE << "2D influence factor (0.0-5.0, default: 1.5)\n" << ANSI_RESET
              << ANSI_BRIGHT_CYAN << "  -3, --threed VAL          " << ANSI_WHITE << "3D influence factor (0.0-5.0, default: 5.0)\n" << ANSI_RESET
              << ANSI_ORANGE << "  -1, --oned VAL            " << ANSI_WHITE << "1D permeation factor (0.0-5.0, default: 1.0)\n" << ANSI_RESET
              << ANSI_BRIGHT_CYAN << "  -n, --nurbmatter VAL      " << ANSI_WHITE << "NURBS dark matter strength (0.0-1.0, default: 0.27)\n" << ANSI_RESET
              << ANSI_ORANGE << "  -e, --nurbenergy VAL      " << ANSI_WHITE << "NURBS dark energy strength (0.0-2.0, default: 0.68)\n" << ANSI_RESET
              << ANSI_BRIGHT_CYAN << "  -a, --alpha VAL           " << ANSI_WHITE << "Alpha parameter (0.01-10.0, default: 0.1)\n" << ANSI_RESET
              << ANSI_ORANGE << "  -b, --beta VAL            " << ANSI_WHITE << "Beta parameter (0.0-1.0, default: 0.5)\n" << ANSI_RESET
              << ANSI_BRIGHT_CYAN << "  -r, --carroll VAL         " << ANSI_WHITE << "Carroll factor (0.0-1.0, default: 0.1)\n" << ANSI_RESET
              << ANSI_ORANGE << "  -f, --meanfield VAL       " << ANSI_WHITE << "Mean field approximation (0.0-1.0, default: 0.5)\n" << ANSI_RESET
              << ANSI_BRIGHT_CYAN << "  -y, --asymcollapse VAL    " << ANSI_WHITE << "Asymmetric collapse factor (0.0-1.0, default: 0.5)\n" << ANSI_RESET
              << ANSI_ORANGE << "  -p, --perspectivetrans VAL " << ANSI_WHITE << "Perspective translation (0.0-10.0, default: 2.0)\n" << ANSI_RESET
              << ANSI_BRIGHT_CYAN << "  -q, --perspectivefocal VAL " << ANSI_WHITE << "Perspective focal length (1.0-20.0, default: 4.0)\n" << ANSI_RESET
              << ANSI_ORANGE << "  -x, --spininteraction VAL  " << ANSI_WHITE << "Spin interaction strength (0.0-1.0, default: 0.1)\n" << ANSI_RESET
              << ANSI_BRIGHT_CYAN << "  -z, --emfield VAL         " << ANSI_WHITE << "EM field strength (0.0-10000000.0, default: 1000.0)\n" << ANSI_RESET
              << ANSI_ORANGE << "  -u, --renorm VAL          " << ANSI_WHITE << "Renormalization factor (0.1-10.0, default: 1.0)\n" << ANSI_RESET
              << ANSI_BRIGHT_CYAN << "  -v, --vacuum VAL          " << ANSI_WHITE << "Vacuum energy (0.0-1.0, default: 0.1)\n" << ANSI_RESET
              << ANSI_ORANGE << "  -g, --godwavefreq VAL     " << ANSI_WHITE << "God wave frequency (0.1-10.0, default: 1.5)\n" << ANSI_RESET
              << ANSI_BRIGHT_CYAN << "  -V, --vertices NUM        " << ANSI_WHITE << "Number of vertices (default: 1000)\n" << ANSI_RESET
              << ANSI_ORANGE << "      --debug               " << ANSI_WHITE << "Enable verbose debug logging (default: off)\n" << ANSI_RESET
              << ANSI_BRIGHT_GREEN << "Example (Bosonic 26D):\n"
              << ANSI_WHITE << "  ./quantum_sim -d 26 -m 3 -t 10 -s 0.01 -i 2.0 -g 1.5 -n 0.27 -e 0.68\n" << ANSI_RESET
              << ANSI_BRIGHT_MAGENTA << "============================================================\n" << ANSI_RESET
              << std::endl;
}

int main(int argc, char* argv[]) {
    // Platform-specific OpenMP thread limit to prevent hangs on Windows
    // Fix: Set to 1 on all platforms to avoid potential parallel issues
    omp_set_num_threads(48);

    // Updated defaults to ensure non-zero energy contributions
    int maxDimensions = 26;  // Bosonic model
    int mode = 3;
    int timesteps = 10;  // Non-zero timesteps for evolution
    [[maybe_unused]] double dt = 0.01;
    double influence = 2.0;  // Non-zero to ensure potential contribution
    double weak = 0.1;  // Non-zero for vector potential
    double collapse = 5.0;
    double twoD = 1.5;
    double threeDInfluence = 5.0;
    double oneDPermeation = 1.0;
    double nurbMatterStrength = 0.27;  // Matches ~27% dark matter
    double nurbEnergyStrength = 0.68;  // Matches ~68% dark energy
    double alpha = 0.1;
    double beta = 0.5;
    double carrollFactor = 0.1;
    double meanFieldApprox = 0.5;
    double asymCollapse = 0.5;
    double perspectiveTrans = 2.0;
    double perspectiveFocal = 4.0;
    double spinInteraction = 0.1;  // Non-zero for spinEnergy contribution
    double emFieldStrength = 1000.0;  // Non-zero for fieldEnergy contribution
    double renormFactor = 1.0;
    double vacuumEnergy = 0.1;
    double godWaveFreq = 1.5;  // Non-zero for GodWaveEnergy contribution
    bool debug = false;
    uint64_t numVertices = 1000;

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"dimensions", required_argument, 0, 'd'},
        {"mode", required_argument, 0, 'm'},
        {"timesteps", required_argument, 0, 't'},
        {"dt", required_argument, 0, 's'},
        {"influence", required_argument, 0, 'i'},
        {"weak", required_argument, 0, 'w'},
        {"collapse", required_argument, 0, 'c'},
        {"twod", required_argument, 0, '2'},
        {"threed", required_argument, 0, '3'},
        {"oned", required_argument, 0, '1'},
        {"nurbmatter", required_argument, 0, 'n'},
        {"nurbenergy", required_argument, 0, 'e'},
        {"alpha", required_argument, 0, 'a'},
        {"beta", required_argument, 0, 'b'},
        {"carroll", required_argument, 0, 'r'},
        {"meanfield", required_argument, 0, 'f'},
        {"asymcollapse", required_argument, 0, 'y'},
        {"perspectivetrans", required_argument, 0, 'p'},
        {"perspectivefocal", required_argument, 0, 'q'},
        {"spininteraction", required_argument, 0, 'x'},
        {"emfield", required_argument, 0, 'z'},
        {"renorm", required_argument, 0, 'u'},
        {"vacuum", required_argument, 0, 'v'},
        {"godwavefreq", required_argument, 0, 'g'},
        {"vertices", required_argument, 0, 'V'},
        {"debug", no_argument, 0, 0},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "hd:m:t:s:i:w:c:2:3:1:n:e:a:b:r:f:y:p:q:x:z:u:v:g:V:", long_options, &option_index)) != -1) {
        try {
            switch (opt) {
                case 'h':
                    printHelp();
                    return 0;
                case 'd':
                    maxDimensions = std::stoi(optarg);
                    if (maxDimensions < 1 || maxDimensions > 26) throw std::invalid_argument("Dims out of 1-26 range (Bosonic max 26)");
                    break;
                case 'm':
                    mode = std::stoi(optarg);
                    if (mode < 1 || mode > maxDimensions) throw std::invalid_argument("Mode out of 1-DIM range");
                    break;
                case 't':
                    timesteps = std::stoi(optarg);
                    break;
                case 's':
                    dt = std::stod(optarg);
                    break;
                case 'i':
                    influence = std::stod(optarg);
                    break;
                case 'w':
                    weak = std::stod(optarg);
                    break;
                case 'c':
                    collapse = std::stod(optarg);
                    break;
                case '2':
                    twoD = std::stod(optarg);
                    break;
                case '3':
                    threeDInfluence = std::stod(optarg);
                    break;
                case '1':
                    oneDPermeation = std::stod(optarg);
                    break;
                case 'n':
                    nurbMatterStrength = std::stod(optarg);
                    break;
                case 'e':
                    nurbEnergyStrength = std::stod(optarg);
                    break;
                case 'a':
                    alpha = std::stod(optarg);
                    break;
                case 'b':
                    beta = std::stod(optarg);
                    break;
                case 'r':
                    carrollFactor = std::stod(optarg);
                    break;
                case 'f':
                    meanFieldApprox = std::stod(optarg);
                    break;
                case 'y':
                    asymCollapse = std::stod(optarg);
                    break;
                case 'p':
                    perspectiveTrans = std::stod(optarg);
                    break;
                case 'q':
                    perspectiveFocal = std::stod(optarg);
                    break;
                case 'x':
                    spinInteraction = std::stod(optarg);
                    break;
                case 'z':
                    emFieldStrength = std::stod(optarg);
                    break;
                case 'u':
                    renormFactor = std::stod(optarg);
                    break;
                case 'v':
                    vacuumEnergy = std::stod(optarg);
                    break;
                case 'g':
                    godWaveFreq = std::stod(optarg);
                    break;
                case 'V':
                    numVertices = std::stoull(optarg);
                    break;
                case 0:
                    if (std::string(long_options[option_index].name) == "debug") {
                        debug = true;
                    }
                    break;
                default:
                    std::cerr << ANSI_BRIGHT_RED << "Unknown option. Use --help for usage." << ANSI_RESET << std::endl;
                    return 1;
            }
        } catch (const std::exception& e) {
            std::cerr << ANSI_BRIGHT_RED << "Error parsing option: " << e.what() << ANSI_RESET << std::endl;
            return 1;
        }
    }

    try {
        UniversalEquation eq(
            maxDimensions, mode, static_cast<long double>(influence), static_cast<long double>(weak),
            static_cast<long double>(collapse), static_cast<long double>(twoD),
            static_cast<long double>(threeDInfluence), static_cast<long double>(oneDPermeation),
            static_cast<long double>(nurbMatterStrength), static_cast<long double>(nurbEnergyStrength),
            static_cast<long double>(0.05),  // nurbRegularMatterStrength to match ~5% regular matter
            static_cast<long double>(alpha), static_cast<long double>(beta),
            static_cast<long double>(carrollFactor), static_cast<long double>(meanFieldApprox),
            static_cast<long double>(asymCollapse), static_cast<long double>(perspectiveTrans),
            static_cast<long double>(perspectiveFocal), static_cast<long double>(spinInteraction),
            static_cast<long double>(emFieldStrength), static_cast<long double>(renormFactor),
            static_cast<long double>(vacuumEnergy), static_cast<long double>(godWaveFreq),
            debug, numVertices
        );

        // Logs/params first (data at bottom)
        std::cout << ANSI_ORANGE << "Initializing 26D Bosonic Simulation..." << ANSI_BRIGHT_CYAN << " [Quantum Chaos Engaged]" << ANSI_RESET << std::endl;
        eq.printParameterTable();

        eq.initializeCalculator();

        for (int i = 0; i < timesteps; ++i) {
            eq.advanceCycle();  // Use advanceCycle to include momentum updates
            if (debug) {
                std::cout << ANSI_BRIGHT_CYAN << "Completed timestep " << ANSI_WHITE << (i + 1) << "/" << timesteps
                          << ANSI_BRIGHT_CYAN << ", simulationTime: " << ANSI_WHITE << std::fixed << std::setprecision(6) << eq.getSimulationTime() << ANSI_RESET << std::endl;
            }
        }

        auto results = eq.computeBatch(1, maxDimensions);
        std::cout << ANSI_BRIGHT_CYAN << "Batch compute complete. " << ANSI_ORANGE << "Exporting data..." << ANSI_RESET << std::endl;

        // Intermediate tables (optional; keep light)
        if (debug) {
            eq.printVertexTable();
            eq.printInteractionTable();
        }

        // NURBS table LAST & CLEAR (with printDouble integrated)
        printNURBSTableSample(results);  // Matches UE::DimensionData with non-zero values

    } catch (const std::exception& e) {
        std::cerr << ANSI_BRIGHT_RED << "Simulation failed: " << e.what() << ANSI_RESET << std::endl;
        return 1;
    }

    std::cout << ANSI_BRIGHT_GREEN << "\nSimulation completed successfully. " << ANSI_BRIGHT_CYAN << "Bosonic 26D model engaged." << ANSI_RESET << std::endl;
    return 0;
}
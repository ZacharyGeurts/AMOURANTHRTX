// main.cpp
// Main entry point for AMOURANTH RTX UE Console 2.30.
// Terminal-based BBS menu in 80's retro style.
// Now fully compilable and linkable.
// Copyright Zachary Geurts 2026 (powered by Grok with Science B*! precision)

#include "ue_init.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <fstream>

// === Fixed: Definition of printDouble (was missing in linking) ===
std::ostream& printDouble(std::ostream& os, double val, int precision) {
    os << std::fixed << std::setprecision(precision) << val;
    return os;
}

// Retro BBS ASCII art banner
const std::string BANNER = R"(
   _____  __  __  _____  _    _  _____  ______  _   _  _____ 
  / ____|/ _|/ _| |  __ \| |  | |/ ____|/ __ \ | \ | |/ ____|
 | (___ | |_| |_  | |__) | |  | | |  __| |  | ||  \| | |  __ 
  \___ \|  _|  _| |  _  /| |  | | | |_ | |  | || . ` | | |_ |
  ____) | | | |   | | \ \| |__| | |__| | |__| || |\  | |__| |
 |_____/|_| |_|   |_|  \_\\____/ \_____|\_____/ |_| \_|\_____|
                                                              
         Universal Equation Simulator - Grok Powered          
         Copyright Zachary Geurts 2026 - All Rights Reserved   
)";

// Function to simulate typing effect for retro feel
void typeText(const std::string& text, int delayMs = 20) {
    for (char c : text) {
        std::cout << c << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    std::cout << std::endl;
}

// Function to clear screen in a cross-platform way
void clearScreen() {
#ifdef _WIN32
    std::system("cls");
#else
    std::system("clear");
#endif
}

// Function to display help
void displayHelp() {
    clearScreen();
    std::cout << ANSI_BRIGHT_MAGENTA << BANNER << ANSI_RESET << std::endl;
    std::cout << ANSI_BOLD << ANSI_CYAN << "AMOURANTH RTX UE Console 2.30 --help" << ANSI_RESET << std::endl;
    std::cout << ANSI_GREEN << "Usage: ./quantum_sim [options]" << ANSI_RESET << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --help          Display this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Run without arguments to enter the interactive retro BBS menu." << std::endl;
    std::cout << "Full control over all dimensions, parameters, computation, and export." << std::endl;
}

// Function to get user input with prompt
std::string getInput(const std::string& prompt) {
    std::cout << ANSI_BRIGHT_YELLOW << prompt << ANSI_RESET;
    std::string input;
    std::getline(std::cin, input);
    return input;
}

// Helper input functions
long double getDouble(const std::string& prompt) {
    while (true) {
        std::string s = getInput(prompt);
        try {
            return std::stold(s);
        } catch (...) {
            std::cout << ANSI_RED << "Invalid number. Try again." << ANSI_RESET << std::endl;
        }
    }
}

int getInt(const std::string& prompt) {
    while (true) {
        std::string s = getInput(prompt);
        try {
            return std::stoi(s);
        } catch (...) {
            std::cout << ANSI_RED << "Invalid integer. Try again." << ANSI_RESET << std::endl;
        }
    }
}

bool getBool(const std::string& prompt) {
    while (true) {
        std::string s = getInput(prompt + " (y/n): ");
        if (s == "y" || s == "Y") return true;
        if (s == "n" || s == "N") return false;
        std::cout << ANSI_RED << "Please enter y or n." << ANSI_RESET << std::endl;
    }
}

uint64_t getUInt64(const std::string& prompt) {
    while (true) {
        std::string s = getInput(prompt);
        try {
            return std::stoull(s);
        } catch (...) {
            std::cout << ANSI_RED << "Invalid unsigned integer. Try again." << ANSI_RESET << std::endl;
        }
    }
}

// Implement missing export methods
void UniversalEquation::exportVertexData(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file) {
        LOG_ERROR_CAT("Export", "Cannot open file: {}", filename);
        return;
    }
    file << "Index,Coordinates,Momentum,Spin,Amplitude\n";
    for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
        file << i << ",";
        for (size_t j = 0; j < nCubeVertices_[i].size(); ++j) {
            file << nCubeVertices_[i][j];
            if (j + 1 < nCubeVertices_[i].size()) file << ";";
        }
        file << ",";
        for (size_t j = 0; j < vertexMomenta_[i].size(); ++j) {
            file << vertexMomenta_[i][j];
            if (j + 1 < vertexMomenta_[i].size()) file << ";";
        }
        file << "," << vertexSpins_[i] << "," << vertexWaveAmplitudes_[i] << "\n";
    }
    LOG_INFO_CAT("Export", "Vertex data exported to {}", filename);
}

void UniversalEquation::exportInteractionData(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file) {
        LOG_ERROR_CAT("Export", "Cannot open file: {}", filename);
        return;
    }
    file << "Vertex,Distance,Strength,VectorPotential,GodWaveAmp\n";
    for (const auto& inter : interactions_) {
        file << inter.vertexIndex << "," << inter.distance << "," << inter.strength << ",";
        for (size_t j = 0; j < inter.vectorPotential.size(); ++j) {
            file << inter.vectorPotential[j];
            if (j + 1 < inter.vectorPotential.size()) file << ";";
        }
        file << "," << inter.godWaveAmplitude << "\n";
    }
    LOG_INFO_CAT("Export", "Interaction data exported to {}", filename);
}

// Main interactive menu
void mainMenu(UniversalEquation& ue) {
    while (true) {
        clearScreen();
        std::cout << ANSI_BRIGHT_MAGENTA << BANNER << ANSI_RESET << std::endl;
        std::cout << ANSI_BOLD << ANSI_BLUE
                  << "=== AMOURANTH RTX UE Console === Dim:" << ue.getCurrentDimension()
                  << " | Vertices:" << ue.getCurrentVertices()
                  << " | GodWave:" << ue.getGodWaveFreq()
                  << ANSI_RESET << std::endl;
        std::cout << ANSI_GREEN
                  << "1. Initialize Calculator\n"
                  << "2. Compute Current Dimension\n"
                  << "3. Compute Batch Dimensions\n"
                  << "4. Update Cache\n"
                  << "5. Export Vertex Data\n"
                  << "6. Export Interaction Data\n"
                  << "7. Print Vertex Table\n"
                  << "8. Print Interaction Table\n"
                  << "9. Print Parameter Table\n"
                  << "10. Print NURBS Table\n"
                  << "11. Set Parameters\n"
                  << "12. Advance Simulation Cycle\n"
                  << "13. View All Current Values\n"
                  << "q. Quit\n"
                  << ANSI_RESET;

        std::string choice = getInput("Select option: ");

        if (choice == "q" || choice == "Q") break;

        if (choice == "1") ue.initializeCalculator();
        else if (choice == "2") {
            UE::EnergyResult res = ue.compute();
            std::cout << ANSI_CYAN << "[RESULT] " << res.toString() << ANSI_RESET << std::endl;
        }
        else if (choice == "3") {
            int start = getInt("Start dimension: ");
            int end = getInt("End dimension: ");
            auto batch = ue.computeBatch(start, end);
            for (const auto& d : batch)
                std::cout << ANSI_CYAN << d.toString() << ANSI_RESET << std::endl;
        }
        else if (choice == "4") {
            UE::DimensionData d = ue.updateCache();
            std::cout << ANSI_CYAN << "[CACHE] " << d.toString() << ANSI_RESET << std::endl;
        }
        else if (choice == "5") ue.exportVertexData(getInput("Vertex filename: "));
        else if (choice == "6") ue.exportInteractionData(getInput("Interaction filename: "));
        else if (choice == "7") ue.printVertexTable();
        else if (choice == "8") ue.printInteractionTable();
        else if (choice == "9") ue.printParameterTable();
        else if (choice == "10") ue.printNURBSTable();
        else if (choice == "12") {
            ue.advanceCycle();
            std::cout << ANSI_GREEN << "Cycle advanced." << ANSI_RESET << std::endl;
        }
        else if (choice == "13") {
            std::cout << ANSI_CYAN << "Current State:" << ANSI_RESET << std::endl;
            std::cout << "Dimension: " << ue.getCurrentDimension() << "\n"
                      << "Mode: " << ue.getMode() << "\n"
                      << "God Wave Freq: " << ue.getGodWaveFreq() << "\n"
                      << "Influence: " << ue.getInfluence() << "\n"
                      << "Weak: " << ue.getWeak() << "\n"
                      << "Vertices: " << ue.getCurrentVertices() << "\n"
                      << "Simulation Time: " << ue.getSimulationTime() << "\n";
        }
        else if (choice == "11") {
            clearScreen();
            std::cout << ANSI_YELLOW << "=== Parameter Adjustment ===" << ANSI_RESET << std::endl;
            ue.setGodWaveFreq(getDouble("God Wave Frequency: "));
            ue.setCurrentDimension(getInt("Current Dimension: "));
            ue.setMode(getInt("Mode: "));
            ue.setInfluence(getDouble("Influence: "));
            ue.setWeak(getDouble("Weak: "));
            ue.setCollapse(getDouble("Collapse: "));
            ue.setTwoD(getDouble("TwoD: "));
            ue.setThreeDInfluence(getDouble("ThreeD Influence: "));
            ue.setOneDPermeation(getDouble("OneD Permeation: "));
            ue.setNurbMatterStrength(getDouble("Nurb Matter Strength: "));
            ue.setNurbEnergyStrength(getDouble("Nurb Energy Strength: "));
            ue.setNurbRegularMatterStrength(getDouble("Nurb Regular Matter Strength: "));
            ue.setAlpha(getDouble("Alpha: "));
            ue.setBeta(getDouble("Beta: "));
            ue.setCarrollFactor(getDouble("Carroll Factor: "));
            ue.setMeanFieldApprox(getDouble("Mean Field Approx: "));
            ue.setAsymCollapse(getDouble("Asym Collapse: "));
            ue.setPerspectiveTrans(getDouble("Perspective Trans: "));
            ue.setPerspectiveFocal(getDouble("Perspective Focal: "));
            ue.setSpinInteraction(getDouble("Spin Interaction: "));
            ue.setEMFieldStrength(getDouble("EM Field Strength: "));
            ue.setRenormFactor(getDouble("Renorm Factor: "));
            ue.setVacuumEnergy(getDouble("Vacuum Energy: "));
            ue.setDebug(getBool("Debug mode"));
            ue.setCurrentVertices(getUInt64("Current Vertices: "));
            ue.setTotalCharge(getDouble("Total Charge: "));
            ue.setMaterialDensity(getDouble("Material Density: "));
            std::cout << ANSI_GREEN << "All parameters updated." << ANSI_RESET << std::endl;
        }
        else {
            std::cout << ANSI_RED << "Invalid selection." << ANSI_RESET << std::endl;
        }

        getInput("\nPress Enter to continue...");
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        displayHelp();
        return 0;
    }

    clearScreen();
    typeText(std::string(ANSI_BRIGHT_MAGENTA) + BANNER + ANSI_RESET, 5);
    typeText(std::string(ANSI_ORANGE) + "Connecting to Universal Equation Simulator...");
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    typeText(std::string(ANSI_GREEN) + "Connection established. Welcome, Operator." + ANSI_RESET);

    UniversalEquation ue(9999, 3, 1.0L, 0.5L, true, 1000);
    ue.initializeCalculator();

    mainMenu(ue);

    typeText(std::string(ANSI_MAGENTA) + "Logging off... Goodbye, Operator." + ANSI_RESET);
    return 0;
}
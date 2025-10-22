// universal_equation.hpp
// 2025 Zachary Geurts
// Header file for the UniversalEquation class, which simulates quantum-like interactions
// in n-dimensional hypercube lattices, modeling emergent behaviors with tunable parameters.
// Purpose: Provides a framework for physics simulations, data analysis, and visualization
// of high-dimensional systems, incorporating concepts like dark matter, dark energy, and
// dimensional collapse. Designed for thread-safety with atomic variables and mutexes.
// Vulkan Integration: Interfaces with Vulkan via DimensionalNavigator for rendering
// simulation results (e.g., energy distributions, vertex interactions). Ensure proper
// Vulkan pipeline setup to avoid errors like VUID-VkGraphicsPipelineCreateInfo-layout-07988
// and VUID-RuntimeSpirv-OpEntryPoint-08743 by matching shader interfaces and descriptor layouts.
// Usage: Include in projects requiring simulation of multidimensional physical systems.
// Example: Create an instance with UniversalEquation(4, 3, 1.0, ...), initialize with
// DimensionalNavigator, and compute energies for rendering.

#ifndef UNIVERSAL_EQUATION_HPP
#define UNIVERSAL_EQUATION_HPP

#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <limits>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <mutex>
#include <atomic>
#include <sstream>
#include <iomanip>

// Forward declaration of DimensionalNavigator to avoid circular dependency
// Purpose: Allows integration with Vulkan-based rendering for visualizing simulation results
// Note: Ensure DimensionalNavigator sets up Vulkan pipeline correctly, including descriptor
// set layouts and shader interfaces, to avoid validation errors
class DimensionalNavigator;

// Inspired by the time-dependent Schrödinger equation: i ħ ∂/∂t Ψ = Ĥ Ψ
// This class simulates emergent behaviors in n-dimensional hypercube lattices,
// permeating probabilities through tunable parameters like influence, weak interactions,
// collapse, and dark components. Thread-safe with atomic variables and mutexes.
// Designed for data scientists: Provides extensive getters for parameters and results,
// with debug logging for traceability.
// Vulkan Usage: Use with DimensionalNavigator to render simulation outputs (e.g., energy
// distributions, vertex interactions) in a Vulkan pipeline. Ensure fragment shader
// descriptors (e.g., envMap) are declared in the pipeline layout and vertex/fragment
// shader interfaces match to avoid validation errors (e.g., VUID-RuntimeSpirv-OpEntryPoint-08743).

class UniversalEquation {
public:
    // Structure to hold energy computation results
    // Purpose: Encapsulates observable, potential, dark matter, and dark energy values
    // Usage: Returned by compute() to provide simulation results for analysis or rendering
    // Vulkan Note: EnergyResult data can be passed to shaders for visualization (e.g., as uniforms)
    struct EnergyResult {
        double observable;    // Observable energy component (directly measurable phenomena)
        double potential;     // Potential energy component (stored energy in the system)
        double darkMatter;    // Dark matter contribution (invisible mass-like effects)
        double darkEnergy;    // Dark energy contribution (expansive force-like effects)
        // Converts results to a formatted string for logging or display
        // Usage: Call toString() to get a readable summary, e.g., result.toString()
        std::string toString() const {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6)
                << "Observable: " << observable
                << ", Potential: " << potential
                << ", Dark Matter: " << darkMatter
                << ", Dark Energy: " << darkEnergy;
            return oss.str();
        }
    };

    // Structure to represent interactions between vertices in the hypercube lattice
    // Purpose: Stores data for each vertex interaction (index, distance, strength)
    // Usage: Accessed via getInteractions() for analysis or rendering of lattice interactions
    // Vulkan Note: Can be used to populate vertex buffers or compute shaders for visualizing
    // lattice interactions
    struct DimensionInteraction {
        int vertexIndex;      // Index of the interacting vertex (relative to reference)
        double distance;      // Euclidean distance to the reference vertex in n-space
        double strength;      // Computed interaction strength (influenced by parameters)
        DimensionInteraction(int idx, double dist, double str)
            : vertexIndex(idx), distance(dist), strength(str) {}
    };

    // Constructor with default parameters for easy instantiation
    // Parameters:
    //   - maxDimensions: Maximum number of dimensions (1 to 20, default 9)
    //   - mode: Initial dimensional mode (1 to maxDimensions, default 1)
    //   - influence: Base interaction strength (0 to 10, default 0.05)
    //   - weak: Weak interaction modifier (0 to 1, default 0.01)
    //   - collapse: Dimensional collapse factor (0 to 5, default 0.1)
    //   - twoD: 2D interaction strength (0 to 5, default 0.0)
    //   - threeDInfluence: 3D-specific interaction strength (0 to 5, default 0.1)
    //   - oneDPermeation: 1D permeation factor (0 to 5, default 0.1)
    //   - darkMatterStrength: Dark matter influence (0 to 1, default 0.27)
    //   - darkEnergyStrength: Dark energy influence (0 to 2, default 0.68)
    //   - alpha: Exponential decay factor (0.1 to 10, default 2.0)
    //   - beta: Permeation scaling factor (0 to 1, default 0.2)
    //   - debug: Enable/disable debug logging (default false)
    // Usage: Instantiate with desired parameters, e.g., UniversalEquation(4, 3, 1.0, ...)
    // Throws std::exception if initialization fails
    // Vulkan Note: Ensure Vulkan instance and surface are created before rendering with
    // DimensionalNavigator to avoid X errors (e.g., BadMatch)
    UniversalEquation(int maxDimensions = 9, int mode = 1, double influence = 0.05,
                      double weak = 0.01, double collapse = 0.1, double twoD = 0.0,
                      double threeDInfluence = 0.1, double oneDPermeation = 0.1,
                      double darkMatterStrength = 0.27, double darkEnergyStrength = 0.68,
                      double alpha = 2.0, double beta = 0.2, bool debug = false);

    // Setters and Getters for all parameters (thread-safe via atomics)
    // Purpose: Allow modification and retrieval of simulation parameters
    // Usage: Use setters to adjust parameters, e.g., setInfluence(2.5), and getters to query, e.g., getInfluence()
    // Vulkan Note: Parameter changes may require updating Vulkan buffers/uniforms in DimensionalNavigator

    // Sets base interaction strength (clamped 0 to 10)
    void setInfluence(double value);
    double getInfluence() const;

    // Sets weak interaction modifier (clamped 0 to 1)
    void setWeak(double value);
    double getWeak() const;

    // Sets dimensional collapse factor (clamped 0 to 5)
    void setCollapse(double value);
    double getCollapse() const;

    // Sets 2D interaction strength (clamped 0 to 5)
    void setTwoD(double value);
    double getTwoD() const;

    // Sets 3D-specific interaction strength (clamped 0 to 5)
    void setThreeDInfluence(double value);
    double getThreeDInfluence() const;

    // Sets 1D permeation factor (clamped 0 to 5)
    void setOneDPermeation(double value);
    double getOneDPermeation() const;

    // Sets dark matter influence (clamped 0 to 1)
    void setDarkMatterStrength(double value);
    double getDarkMatterStrength() const;

    // Sets dark energy influence (clamped 0 to 2)
    void setDarkEnergyStrength(double value);
    double getDarkEnergyStrength() const;

    // Sets exponential decay factor (clamped 0.1 to 10)
    void setAlpha(double value);
    double getAlpha() const;

    // Sets permeation scaling factor (clamped 0 to 1)
    void setBeta(double value);
    double getBeta() const;

    // Sets debug logging state
    void setDebug(bool value);
    bool getDebug() const;

    // Sets simulation mode (clamped 1 to maxDimensions)
    void setMode(int mode);
    int getMode() const;

    // Sets current dimension (clamped 1 to maxDimensions)
    void setCurrentDimension(int dimension);
    int getCurrentDimension() const;

    // Gets maximum number of dimensions
    int getMaxDimensions() const;

    // Additional getters for internal state (useful for data analysis and rendering)
    // Purpose: Provide access to simulation state for analysis and visualization
    // Usage: Query internal parameters, e.g., getOmega(), getInteractions()
    // Vulkan Note: Use getNCubeVertices() and getInteractions() to populate vertex buffers
    // or compute shaders for rendering lattice structures

    // Gets angular frequency for 2D oscillation
    double getOmega() const;

    // Gets inverse of max dimensions for scaling
    double getInvMaxDim() const;

    // Gets interaction data (thread-safe)
    const std::vector<DimensionInteraction>& getInteractions() const;

    // Gets hypercube vertices (thread-safe)
    const std::vector<std::vector<double>>& getNCubeVertices() const;

    // Gets precomputed cosine values (thread-safe)
    const std::vector<double>& getCachedCos() const;

    // Advances the simulation to the next dimension (cycles back to 1 if at max)
    // Usage: Iterate through dimensions, e.g., advanceCycle()
    // Vulkan Note: Update Vulkan buffers after advancing to reflect new dimension
    void advanceCycle();

    // Computes energy components for the current dimension
    // Returns: EnergyResult with observable, potential, dark matter, and dark energy
    // Usage: Run simulation and retrieve results, e.g., auto result = compute()
    // Vulkan Note: Pass EnergyResult to shaders (e.g., as uniforms) for visualization
    EnergyResult compute() const;

    // Initializes calculator with a navigator for rendering integration
    // Parameters:
    //   - navigator: Pointer to DimensionalNavigator for visualization
    // Usage: Set up rendering, e.g., initializeCalculator(new DimensionalNavigator(...))
    // Throws std::invalid_argument if navigator is null
    // Vulkan Note: Ensure Vulkan instance, device, and surface are properly set up in
    // DimensionalNavigator to avoid errors like BadMatch. Verify descriptor set layouts
    // and shader interfaces match pipeline requirements.
    void initializeCalculator(DimensionalNavigator* navigator);

    // Structure to cache simulation results for data analysis
    // Purpose: Stores dimension and energy results for easy access
    // Usage: Retrieved via updateCache(), e.g., auto data = updateCache()
    // Vulkan Note: DimensionData can be used to update Vulkan uniforms or buffers
    struct DimensionData {
        int dimension;        // Current dimension
        double observable;    // Observable energy
        double potential;     // Potential energy
        double darkMatter;    // Dark matter contribution
        double darkEnergy;    // Dark energy contribution
        // Converts data to a formatted string for logging or display
        std::string toString() const {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6)
                << "Dimension: " << dimension
                << ", Observable: " << observable
                << ", Potential: " << potential
                << ", Dark Matter: " << darkMatter
                << ", Dark Energy: " << darkEnergy;
            return oss.str();
        }
    };

    // Updates and returns cached simulation data
    // Usage: Retrieve current simulation state, e.g., auto data = updateCache()
    // Vulkan Note: Use DimensionData to update rendering data in Vulkan pipeline
    DimensionData updateCache();

    // Public methods for AMOURANTH access (rendering integration)
    // Purpose: Expose internal calculations for external rendering or analysis
    // Usage: Used by DimensionalNavigator or rendering systems to compute shader inputs
    // Vulkan Note: Ensure shader interfaces using these methods have matching descriptor
    // layouts and vertex/fragment outputs to avoid validation errors

    // Computes interaction strength for a vertex at a given distance
    double computeInteraction(int vertexIndex, double distance) const;

    // Computes permeation factor for a vertex
    double computePermeation(int vertexIndex) const;

    // Computes dark energy contribution based on distance
    double computeDarkEnergy(double distance) const;

private:
    // Private members for internal state (thread-safe where necessary)
    int maxDimensions_;          // Maximum number of dimensions (1 to 20, constant after init)
    std::atomic<int> currentDimension_; // Current dimension for computation
    std::atomic<int> mode_;      // Current mode (aligned with currentDimension_)
    uint64_t maxVertices_;       // Maximum number of vertices (2^20, constant after init)
    std::atomic<double> influence_; // Base interaction strength (0 to 10)
    std::atomic<double> weak_;   // Weak interaction strength (0 to 1)
    std::atomic<double> collapse_; // Dimensional collapse factor (0 to 5)
    std::atomic<double> twoD_;   // 2D influence factor (0 to 5)
    std::atomic<double> threeDInfluence_; // 3D influence factor (0 to 5)
    std::atomic<double> oneDPermeation_; // 1D permeation factor (0 to 5)
    std::atomic<double> darkMatterStrength_; // Dark matter strength (0 to 1)
    std::atomic<double> darkEnergyStrength_; // Dark energy strength (0 to 2)
    std::atomic<double> alpha_;  // Exponential decay factor (0.1 to 10)
    std::atomic<double> beta_;   // Permeation scaling factor (0 to 1)
    std::atomic<bool> debug_;    // Debug output flag
    double omega_;               // Angular frequency for 2D oscillation (constant after init)
    double invMaxDim_;           // Inverse of max dimensions for scaling (constant after init)
    mutable std::vector<DimensionInteraction> interactions_; // Interaction data (protected by mutex_)
    std::vector<std::vector<double>> nCubeVertices_; // Vertex coordinates for n-cube (protected by mutex_)
    mutable std::atomic<bool> needsUpdate_; // Dirty flag for interactions
    std::vector<double> cachedCos_; // Cached cosine values (protected by mutex_)
    DimensionalNavigator* navigator_; // Pointer to DimensionalNavigator (managed externally)
    mutable std::mutex mutex_; // Protects interactions_, nCubeVertices_, cachedCos_
    mutable std::mutex debugMutex_; // Protects debug output for thread-safe logging

    // Private methods for internal computations
    // Computes the collapse factor based on dimension and parameters
    // Usage: Called during compute() to apply dimensional convergence effects
    double computeCollapse() const;

    // Initializes n-cube vertices as binary combinations (±1) in n-space
    // Usage: Called during initialization to set up the simulation lattice
    void initializeNCube();

    // Updates interaction data (vertex index, distance, strength) for the current dimension
    // Usage: Called when needsUpdate_ is true to refresh simulation state
    void updateInteractions() const;

    // Initializes hypercube and cosines with retry logic for memory errors
    // Usage: Called during constructor and dimension/mode changes
    void initializeWithRetry();

    // Safe exponential function to prevent overflow/underflow
    // Usage: Used in interaction and dark energy calculations
    double safeExp(double x) const;
};

#endif // UNIVERSAL_EQUATION_HPP
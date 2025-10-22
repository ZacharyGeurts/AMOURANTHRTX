// UniversalEquation.hpp implementation
// 2025 Zachary Geurts
// This class simulates a universal equation for modeling physical interactions across multiple dimensions
// in a hypercube lattice, incorporating concepts like dark matter, dark energy, and dimensional collapse.
// Used for physics simulations, data analysis, and visualization of high-dimensional systems.
// Thread-safe with mutex locks for shared resources.
// Usage: Instantiate with desired parameters, set modes/dimensions, and compute energy results.

#include "universal_equation.hpp"
#ifdef _WIN32
#define M_PI 3.14159265358979323846264338327950288
#endif
// Constructor: Initializes the UniversalEquation with parameters for dimensional simulation
// Parameters:
//   - maxDimensions: Maximum number of dimensions (1 to 20, default 20 if 0)
//   - mode: Initial dimensional mode (clamped to 1 to maxDimensions)
//   - influence: Base interaction strength (0 to 10)
//   - weak: Weak interaction modifier (0 to 1)
//   - collapse: Dimensional collapse factor (0 to 5)
//   - twoD: 2D interaction strength (0 to 5)
//   - threeDInfluence: 3D-specific interaction strength (0 to 5)
//   - oneDPermeation: 1D permeation factor (0 to 5)
//   - darkMatterStrength: Dark matter influence (0 to 1)
//   - darkEnergyStrength: Dark energy influence (0 to 2)
//   - alpha: Exponential decay factor (0.1 to 10)
//   - beta: Permeation scaling factor (0 to 1)
//   - debug: Enable/disable debug logging
// Usage: Create an instance with desired parameters, e.g., UniversalEquation(4, 3, 1.0, ...)
// Throws std::exception on initialization failure
UniversalEquation::UniversalEquation(int maxDimensions, int mode, double influence,
                                     double weak, double collapse, double twoD,
                                     double threeDInfluence, double oneDPermeation,
                                     double darkMatterStrength, double darkEnergyStrength,
                                     double alpha, double beta, bool debug)
    : maxDimensions_(std::max(1, std::min(maxDimensions == 0 ? 20 : maxDimensions, 20))),
      currentDimension_(std::clamp(mode, 1, maxDimensions_)),
      mode_(std::clamp(mode, 1, maxDimensions_)),
      maxVertices_(1ULL << std::min(maxDimensions_, 20)),
      influence_(std::clamp(influence, 0.0, 10.0)),
      weak_(std::clamp(weak, 0.0, 1.0)),
      collapse_(std::clamp(collapse, 0.0, 5.0)),
      twoD_(std::clamp(twoD, 0.0, 5.0)),
      threeDInfluence_(std::clamp(threeDInfluence, 0.0, 5.0)),
      oneDPermeation_(std::clamp(oneDPermeation, 0.0, 5.0)),
      darkMatterStrength_(std::clamp(darkMatterStrength, 0.0, 1.0)),
      darkEnergyStrength_(std::clamp(darkEnergyStrength, 0.0, 2.0)),
      alpha_(std::clamp(alpha, 0.1, 10.0)),
      beta_(std::clamp(beta, 0.0, 1.0)),
      debug_(debug),
      omega_(maxDimensions_ > 0 ? 2.0 * M_PI / (2 * maxDimensions_ - 1) : 1.0), // Angular frequency for oscillations
      invMaxDim_(maxDimensions_ > 0 ? 1.0 / maxDimensions_ : 1e-15), // Inverse max dimension for scaling
      interactions_(), // Stores interaction data (vertex index, distance, strength)
      nCubeVertices_(), // Stores hypercube vertices as binary coordinates
      needsUpdate_(true), // Flag to trigger interaction updates
      navigator_(nullptr) { // Pointer to rendering navigator (optional)
    // Initialize with retry to handle memory allocation for large dimensions
    try {
        initializeWithRetry();
        if (debug_.load()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[DEBUG] Initialized: maxDimensions=" << maxDimensions_ << ", mode=" << mode_.load()
                      << ", currentDimension=" << currentDimension_.load() << ", maxVertices=" << maxVertices_ << "\n";
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] Constructor failed: " << e.what() << "\n";
        throw;
    }
}

// Sets the base interaction strength (clamped 0 to 10)
// Usage: Adjust influence for simulation, e.g., setInfluence(2.5)
void UniversalEquation::setInfluence(double value) {
    influence_.store(std::clamp(value, 0.0, 10.0));
    needsUpdate_.store(true); // Mark for interaction update
}

// Gets the current base interaction strength
// Usage: Retrieve current influence, e.g., double inf = getInfluence()
double UniversalEquation::getInfluence() const { return influence_.load(); }

// Sets the weak interaction modifier (clamped 0 to 1)
// Usage: Adjust weak interaction scaling, e.g., setWeak(0.5)
void UniversalEquation::setWeak(double value) {
    weak_.store(std::clamp(value, 0.0, 1.0));
    needsUpdate_.store(true);
}

// Gets the current weak interaction modifier
double UniversalEquation::getWeak() const { return weak_.load(); }

// Sets the dimensional collapse factor (clamped 0 to 5)
// Usage: Adjust collapse strength, e.g., setCollapse(1.0)
void UniversalEquation::setCollapse(double value) {
    collapse_.store(std::clamp(value, 0.0, 5.0));
}

// Gets the current collapse factor
double UniversalEquation::getCollapse() const { return collapse_.load(); }

// Sets the 2D interaction strength (clamped 0 to 5)
// Usage: Adjust 2D-specific interactions, e.g., setTwoD(2.0)
void UniversalEquation::setTwoD(double value) {
    twoD_.store(std::clamp(value, 0.0, 5.0));
    needsUpdate_.store(true);
}

// Gets the current 2D interaction strength
double UniversalEquation::getTwoD() const { return twoD_.load(); }

// Sets the 3D-specific interaction strength (clamped 0 to 5)
// Usage: Adjust 3D-specific effects, e.g., setThreeDInfluence(3.0)
void UniversalEquation::setThreeDInfluence(double value) {
    threeDInfluence_.store(std::clamp(value, 0.0, 5.0));
    needsUpdate_.store(true);
}

// Gets the current 3D interaction strength
double UniversalEquation::getThreeDInfluence() const { return threeDInfluence_.load(); }

// Sets the 1D permeation factor (clamped 0 to 5)
// Usage: Adjust 1D probability flow, e.g., setOneDPermeation(1.5)
void UniversalEquation::setOneDPermeation(double value) {
    oneDPermeation_.store(std::clamp(value, 0.0, 5.0));
    needsUpdate_.store(true);
}

// Gets the current 1D permeation factor
double UniversalEquation::getOneDPermeation() const { return oneDPermeation_.load(); }

// Sets the dark matter influence (clamped 0 to 1)
// Usage: Adjust dark matter effect, e.g., setDarkMatterStrength(0.7)
void UniversalEquation::setDarkMatterStrength(double value) {
    darkMatterStrength_.store(std::clamp(value, 0.0, 1.0));
    needsUpdate_.store(true);
}

// Gets the current dark matter strength
double UniversalEquation::getDarkMatterStrength() const { return darkMatterStrength_.load(); }

// Sets the dark energy influence (clamped 0 to 2)
// Usage: Adjust dark energy effect, e.g., setDarkEnergyStrength(1.2)
void UniversalEquation::setDarkEnergyStrength(double value) {
    darkEnergyStrength_.store(std::clamp(value, 0.0, 2.0));
    needsUpdate_.store(true);
}

// Gets the current dark energy strength
double UniversalEquation::getDarkEnergyStrength() const { return darkEnergyStrength_.load(); }

// Sets the exponential decay factor (clamped 0.1 to 10)
// Usage: Adjust interaction decay rate, e.g., setAlpha(5.0)
void UniversalEquation::setAlpha(double value) {
    alpha_.store(std::clamp(value, 0.1, 10.0));
    needsUpdate_.store(true);
}

// Gets the current exponential decay factor
double UniversalEquation::getAlpha() const { return alpha_.load(); }

// Sets the permeation scaling factor (clamped 0 to 1)
// Usage: Adjust vertex magnitude scaling, e.g., setBeta(0.3)
void UniversalEquation::setBeta(double value) {
    beta_.store(std::clamp(value, 0.0, 1.0));
    needsUpdate_.store(true);
}

// Gets the current permeation scaling factor
double UniversalEquation::getBeta() const { return beta_.load(); }

// Sets debug logging state
// Usage: Enable/disable debug output, e.g., setDebug(true)
void UniversalEquation::setDebug(bool value) { debug_.store(value); }

// Gets the current debug state
bool UniversalEquation::getDebug() const { return debug_.load(); }

// Sets the simulation mode (clamped 1 to maxDimensions)
// Usage: Switch dimensional mode, e.g., setMode(3)
void UniversalEquation::setMode(int mode) {
    mode = std::clamp(mode, 1, maxDimensions_);
    if (mode_.load() != mode || currentDimension_.load() != mode) {
        mode_.store(mode);
        currentDimension_.store(mode);
        needsUpdate_.store(true);
        initializeWithRetry(); // Reinitialize hypercube and interactions
        if (debug_.load()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[DEBUG] Mode set to: " << mode_.load() << ", dimension: " << currentDimension_.load() << "\n";
        }
    }
}

// Gets the current simulation mode
int UniversalEquation::getMode() const { return mode_.load(); }

// Sets the current dimension (clamped 1 to maxDimensions)
// Usage: Change active dimension, e.g., setCurrentDimension(4)
void UniversalEquation::setCurrentDimension(int dimension) {
    dimension = std::clamp(dimension, 1, maxDimensions_);
    if (dimension != currentDimension_.load()) {
        currentDimension_.store(dimension);
        mode_.store(dimension);
        needsUpdate_.store(true);
        initializeWithRetry(); // Reinitialize for new dimension
        if (debug_.load()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[DEBUG] Dimension set to: " << currentDimension_.load() << ", mode: " << mode_.load() << "\n";
        }
    }
}

// Gets the current dimension
int UniversalEquation::getCurrentDimension() const { return currentDimension_.load(); }

// Gets the maximum number of dimensions
int UniversalEquation::getMaxDimensions() const { return maxDimensions_; }

// Gets the angular frequency for oscillations
double UniversalEquation::getOmega() const { return omega_; }

// Gets the inverse maximum dimension for scaling
double UniversalEquation::getInvMaxDim() const { return invMaxDim_; }

// Gets the current interaction data (thread-safe)
// Usage: Access interactions for analysis, e.g., auto interactions = getInteractions()
const std::vector<UniversalEquation::DimensionInteraction>& UniversalEquation::getInteractions() const {
    if (needsUpdate_.load()) {
        if (debug_.load()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[DEBUG] Updating interactions for dimension: " << currentDimension_.load() << "\n";
        }
        updateInteractions(); // Recompute interactions if needed
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return interactions_;
}

// Gets the hypercube vertices (thread-safe)
// Usage: Access vertex coordinates, e.g., auto vertices = getNCubeVertices()
const std::vector<std::vector<double>>& UniversalEquation::getNCubeVertices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nCubeVertices_;
}

// Gets the precomputed cosine values (thread-safe)
// Usage: Access cached cosines for oscillation calculations
const std::vector<double>& UniversalEquation::getCachedCos() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cachedCos_;
}

// Advances the simulation to the next dimension (cycles back to 1 if at max)
// Usage: Iterate through dimensions, e.g., advanceCycle()
void UniversalEquation::advanceCycle() {
    int newDimension = (currentDimension_.load() == maxDimensions_) ? 1 : currentDimension_.load() + 1;
    currentDimension_.store(newDimension);
    mode_.store(newDimension);
    needsUpdate_.store(true);
    initializeWithRetry(); // Reinitialize for new dimension
    if (debug_.load()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Cycle advanced: dimension=" << currentDimension_.load() << ", mode=" << mode_.load() << "\n";
    }
}

// Computes energy results for the current dimension
// Returns EnergyResult struct with observable, potential, dark matter, and dark energy components
// Usage: Compute simulation results, e.g., auto result = compute()
// Thread-safe with interaction summation for large datasets
UniversalEquation::EnergyResult UniversalEquation::compute() const {
    if (debug_.load()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Starting compute for dimension: " << currentDimension_.load() << "\n";
    }
    if (needsUpdate_.load()) {
        updateInteractions(); // Ensure interactions are up-to-date
    }
    double observable = influence_.load();
    int currDim = currentDimension_.load();
    if (currDim >= 2) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cachedCos_.empty()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] cachedCos_ is empty in compute\n";
            throw std::runtime_error("cachedCos_ is empty");
        }
        observable += twoD_.load() * cachedCos_[currDim % cachedCos_.size()]; // Add 2D oscillation effect
    }
    if (currDim == 3) {
        observable += threeDInfluence_.load(); // Add 3D-specific effect
    }

    double totalDarkMatter = 0.0, totalDarkEnergy = 0.0, interactionSum = 0.0;
    {
        std::vector<DimensionInteraction> localInteractions;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            localInteractions = interactions_; // Copy for thread-safe access
        }
        // Sequential summation for interaction counts
        for (const auto& interaction : localInteractions) {
            double influence = interaction.strength;
            double permeation = computePermeation(interaction.vertexIndex);
            double darkMatter = darkMatterStrength_.load();
            double darkEnergy = computeDarkEnergy(interaction.distance);
            interactionSum += influence * safeExp(-alpha_.load() * interaction.distance) * permeation * darkMatter;
            totalDarkMatter += darkMatter * influence * permeation;
            totalDarkEnergy += darkEnergy * influence * permeation;
        }
    }
    observable += interactionSum;

    double collapse = computeCollapse(); // Apply dimensional collapse effect
    EnergyResult result = {
        observable + collapse, // Total observable energy
        std::max(0.0, observable - collapse), // Potential energy (non-negative)
        totalDarkMatter, // Accumulated dark matter contribution
        totalDarkEnergy // Accumulated dark energy contribution
    };

    if (debug_.load()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Compute(D=" << currDim << "): " << result.toString() << "\n";
    }
    return result;
}

// Computes interaction strength for a vertex at a given distance
// Parameters:
//   - vertexIndex: Index of the vertex in the hypercube
//   - distance: Euclidean distance from reference vertex
// Returns: Interaction strength scaled by dimension, weak modifier, and 3D effects
// Usage: Called internally during interaction updates
double UniversalEquation::computeInteraction(int vertexIndex, double distance) const {
    double denom = std::max(1e-15, std::pow(static_cast<double>(currentDimension_.load()), (vertexIndex % maxDimensions_ + 1)));
    double modifier = (currentDimension_.load() > 3 && (vertexIndex % maxDimensions_ + 1) > 3) ? weak_.load() : 1.0;
    if (currentDimension_.load() == 3 && ((vertexIndex % maxDimensions_ + 1) == 2 || (vertexIndex % maxDimensions_ + 1) == 4)) {
        modifier *= threeDInfluence_.load(); // Enhance 3D interactions for specific vertices
    }
    double result = influence_.load() * (1.0 / (denom * (1.0 + distance))) * modifier;
    if (debug_.load() && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Interaction(vertex=" << vertexIndex << ", dist=" << distance << "): " << result << "\n";
    }
    return result;
}

// Computes permeation factor for a vertex, simulating probability flow
// Parameters:
//   - vertexIndex: Index of the vertex in the hypercube
// Returns: Permeation factor based on dimension and vertex magnitude
// Usage: Called during energy computation to scale interactions
// Throws std::out_of_range if vertex index is invalid
double UniversalEquation::computePermeation(int vertexIndex) const {
    if (vertexIndex < 0 || nCubeVertices_.empty()) {
        if (debug_.load()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Invalid vertex index " << vertexIndex << " or empty vertex list\n";
        }
        throw std::out_of_range("Invalid vertex index or empty vertex list");
    }
    if (vertexIndex == 1 || currentDimension_.load() == 1) return oneDPermeation_.load(); // 1D case
    if (currentDimension_.load() == 2 && (vertexIndex % maxDimensions_ + 1) > 2) return twoD_.load(); // 2D case
    if (currentDimension_.load() == 3 && ((vertexIndex % maxDimensions_ + 1) == 2 || (vertexIndex % maxDimensions_ + 1) == 4)) {
        return threeDInfluence_.load(); // 3D special case
    }

    size_t safeIndex = static_cast<size_t>(vertexIndex) % nCubeVertices_.size();
    std::vector<double> vertex;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (safeIndex >= nCubeVertices_.size()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Invalid vertex index " << safeIndex << " in nCubeVertices_\n";
            throw std::out_of_range("Invalid vertex index");
        }
        vertex = nCubeVertices_[safeIndex];
    }
    double vertexMagnitude = 0.0;
    // Sequential magnitude calculation
    for (size_t i = 0; i < std::min(static_cast<size_t>(currentDimension_.load()), vertex.size()); ++i) {
        vertexMagnitude += vertex[i] * vertex[i];
    }
    vertexMagnitude = std::sqrt(vertexMagnitude);
    double result = 1.0 + beta_.load() * vertexMagnitude / std::max(1, currentDimension_.load());
    if (debug_.load() && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Permeation(vertex=" << vertexIndex << "): " << result << "\n";
    }
    return result;
}

// Computes dark energy contribution based on distance
// Parameters:
//   - distance: Distance from reference vertex
// Returns: Dark energy strength scaled by distance and dimension
// Usage: Called during energy computation
double UniversalEquation::computeDarkEnergy(double distance) const {
    double d = std::min(distance, 10.0); // Clamp distance to avoid extreme values
    double result = darkEnergyStrength_.load() * safeExp(d * invMaxDim_);
    if (debug_.load() && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] DarkEnergy(dist=" << distance << "): " << result << "\n";
    }
    return result;
}

// Computes dimensional collapse factor, simulating wave function collapse
// Returns: Collapse effect based on dimension and oscillation
// Usage: Called during energy computation
// Throws std::runtime_error if cachedCos_ is empty
double UniversalEquation::computeCollapse() const {
    if (currentDimension_.load() == 1) return 0.0; // No collapse in 1D
    double phase = static_cast<double>(currentDimension_.load()) / (2 * maxDimensions_);
    std::lock_guard<std::mutex> lock(mutex_);
    if (cachedCos_.empty()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] cachedCos_ is empty in computeCollapse\n";
        throw std::runtime_error("cachedCos_ is empty");
    }
    double osc = std::abs(cachedCos_[static_cast<size_t>(2.0 * M_PI * phase * cachedCos_.size()) % cachedCos_.size()]);
    double result = std::max(0.0, collapse_.load() * currentDimension_.load() * safeExp(-beta_.load() * (currentDimension_.load() - 1)) * (0.8 * osc + 0.2));
    if (debug_.load() && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Collapse(D=" << currentDimension_.load() << "): " << result << "\n";
    }
    return result;
}

// Initializes hypercube vertices as binary combinations (±1) in n-dimensional space
// Usage: Called during initialization to set up the simulation lattice
void UniversalEquation::initializeNCube() {
    std::lock_guard<std::mutex> lock(mutex_);
    nCubeVertices_.clear();
    uint64_t numVertices = std::min(static_cast<uint64_t>(1ULL << currentDimension_.load()), maxVertices_);
    nCubeVertices_.reserve(numVertices);
    if (debug_.load()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Initializing nCube with " << numVertices << " vertices for dimension " << currentDimension_.load() << "\n";
    }
    // Sequential vertex generation
    for (uint64_t i = 0; i < numVertices; ++i) {
        std::vector<double> vertex(currentDimension_.load(), 0.0);
        for (int j = 0; j < currentDimension_.load(); ++j) {
            vertex[j] = (i & (1ULL << j)) ? 1.0 : -1.0; // Binary coordinates (±1)
        }
        nCubeVertices_.push_back(std::move(vertex));
    }
    if (debug_.load() && nCubeVertices_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Initialized nCube with " << nCubeVertices_.size() << " vertices for dimension " << currentDimension_.load() << "\n";
    }
}

// Updates interaction data (vertex index, distance, strength) for the current dimension
// Usage: Called when needsUpdate_ is true to refresh simulation state
void UniversalEquation::updateInteractions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    interactions_.clear();
    uint64_t numVertices = std::min(static_cast<uint64_t>(1ULL << currentDimension_.load()), maxVertices_);
    interactions_.reserve(numVertices - 1);
    if (nCubeVertices_.empty()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] nCubeVertices_ is empty in updateInteractions\n";
        throw std::runtime_error("nCubeVertices_ is empty");
    }
    const auto& referenceVertex = nCubeVertices_[0]; // Reference vertex for distance calculations
    if (debug_.load()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Updating interactions for " << numVertices - 1 << " vertices\n";
    }
    // Sequential interaction computation
    for (uint64_t i = 1; i < numVertices; ++i) {
        double distance = 0.0;
        for (int j = 0; j < currentDimension_.load(); ++j) {
            double diff = nCubeVertices_[i][j] - referenceVertex[j];
            distance += diff * diff;
        }
        distance = std::sqrt(distance);
        double strength = computeInteraction(static_cast<int>(i), distance);
        interactions_.push_back(DimensionInteraction(static_cast<int>(i), distance, strength));
    }
    // Add specific 3D interactions for vertices 2 and 4
    if (currentDimension_.load() == 3) {
        for (int adj : {2, 4}) {
            size_t vertexIndex = static_cast<size_t>(adj - 1);
            if (vertexIndex < nCubeVertices_.size() &&
                std::none_of(interactions_.begin(), interactions_.end(),
                             [adj](const auto& i) { return i.vertexIndex == adj; })) {
                double distance = 0.0;
                for (int j = 0; j < currentDimension_.load(); ++j) {
                    double diff = nCubeVertices_[vertexIndex][j] - referenceVertex[j];
                    distance += diff * diff;
                }
                distance = std::sqrt(distance);
                double strength = computeInteraction(static_cast<int>(vertexIndex), distance);
                interactions_.push_back(DimensionInteraction(static_cast<int>(vertexIndex), distance, strength));
            }
        }
    }
    needsUpdate_.store(false); // Mark interactions as updated
    if (debug_.load() && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Interactions(D=" << currentDimension_.load() << "): ";
        for (const auto& i : interactions_) {
            std::cout << "(vertex=" << i.vertexIndex << ", dist=" << i.distance << ", strength=" << i.strength << ") ";
        }
        std::cout << "\n";
    }
}

// Initializes the hypercube and precomputes cosines, with retries on memory failure
// Usage: Called during constructor and dimension/mode changes
// Reduces dimension if allocation fails, ensuring robust initialization
void UniversalEquation::initializeWithRetry() {
    while (currentDimension_.load() >= 1) {
        try {
            initializeNCube(); // Generate hypercube vertices
            {
                std::lock_guard<std::mutex> lock(mutex_);
                cachedCos_.resize(maxDimensions_ + 1);
                // Sequential cosine precomputation
                for (int i = 0; i <= maxDimensions_; ++i) {
                    cachedCos_[i] = std::cos(omega_ * i);
                }
            }
            updateInteractions(); // Compute initial interactions
            if (debug_.load()) {
                std::lock_guard<std::mutex> lock(debugMutex_);
                std::cout << "[DEBUG] initializeWithRetry completed for dimension " << currentDimension_.load() << "\n";
            }
            return;
        } catch (const std::bad_alloc& e) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Failed to allocate memory for " << (1ULL << currentDimension_.load())
                      << " vertices. Reducing dimension to " << (currentDimension_.load() - 1) << ".\n";
            if (currentDimension_.load() == 1) {
                throw std::runtime_error("Failed to allocate memory even at dimension 1");
            }
            currentDimension_.store(currentDimension_.load() - 1);
            mode_.store(currentDimension_.load());
            maxVertices_ = 1ULL << std::min(currentDimension_.load(), 20);
            needsUpdate_.store(true);
        }
    }
}

// Safe exponential function to prevent overflow/underflow
// Parameters:
//   - x: Input value
// Returns: Clamped exponential result
// Usage: Used in interaction and dark energy calculations
double UniversalEquation::safeExp(double x) const {
    double result = std::exp(std::clamp(x, -709.0, 709.0));
    if (debug_.load() && (std::isnan(result) || std::isinf(result))) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] safeExp produced invalid result for x=" << x << "\n";
    }
    return result;
}

// Initializes the calculator with a rendering navigator
// Parameters:
//   - navigator: Pointer to DimensionalNavigator for visualization integration
// Usage: Set up rendering, e.g., initializeCalculator(new DimensionalNavigator(...))
// Throws std::invalid_argument if navigator is null
void UniversalEquation::initializeCalculator(DimensionalNavigator* navigator) {
    std::lock_guard<std::mutex> lock(mutex_);
    navigator_ = navigator;
    if (!navigator_) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] Navigator pointer is null\n";
        throw std::invalid_argument("Navigator pointer cannot be null");
    }
    needsUpdate_.store(true);
    initializeWithRetry(); // Reinitialize with navigator
}

// Updates and returns cached simulation data
// Returns: DimensionData struct with dimension, observable, potential, dark matter, and dark energy
// Usage: Retrieve current simulation state, e.g., auto data = updateCache()
UniversalEquation::DimensionData UniversalEquation::updateCache() {
    if (debug_.load()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Starting updateCache for dimension: " << currentDimension_.load() << "\n";
    }
    auto result = compute(); // Compute current energy results
    DimensionData data;
    data.dimension = currentDimension_.load();
    data.observable = result.observable;
    data.potential = result.potential;
    data.darkMatter = result.darkMatter;
    data.darkEnergy = result.darkEnergy;
    if (debug_.load()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] updateCache completed: " << data.toString() << "\n";
    }
    return data;
}
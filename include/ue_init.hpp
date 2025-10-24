// ue_init.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Core initialization and management for the Universal Equation engine
// Dependencies: GLM, Vulkan 1.3+, C++20 standard library, camera.hpp, logging.hpp
// Supported platforms: Linux, Windows
// Zachary Geurts 2025

#pragma once
#ifndef UE_INIT_HPP
#define UE_INIT_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <string>
#include <source_location>
#include <sstream>
#include <format>
#include <algorithm>
#include <iomanip>
#include <numeric> // Added for std::accumulate
#include "engine/camera.hpp"
#include "engine/logging.hpp"

// Forward declarations
namespace VulkanRTX {
class VulkanRenderer;
class VulkanRTX_Setup;
}

namespace UE {

struct DimensionData {
    int dimension = 0;
    long double scale = 1.0L;
    glm::vec3 position = glm::vec3(0.0f);
    float value = 1.0f;
    long double nurbEnergy = 1.0L;
    long double nurbMatter = 0.032774L;
    long double potential = 1.0L;
    long double observable = 1.0L;
    long double spinEnergy = 0.0L;
    long double momentumEnergy = 0.0L;
    long double fieldEnergy = 0.0L;
    long double GodWaveEnergy = 0.0L;

    std::string toString() const {
        return std::format("Dimension: {}, Scale: {:.6Lf}, Position: ({:.3f}, {:.3f}, {:.3f}), Value: {:.3f}, "
                           "NurbEnergy: {:.6Lf}, NurbMatter: {:.6Lf}, Potential: {:.6Lf}, Observable: {:.6Lf}, "
                           "SpinEnergy: {:.6Lf}, MomentumEnergy: {:.6Lf}, FieldEnergy: {:.6Lf}, GodWaveEnergy: {:.6Lf}",
                           dimension, scale, position.x, position.y, position.z, value,
                           nurbEnergy, nurbMatter, potential, observable,
                           spinEnergy, momentumEnergy, fieldEnergy, GodWaveEnergy);
    }
};

inline bool operator==(const DimensionData& lhs, const DimensionData& rhs) noexcept {
    return lhs.dimension == rhs.dimension &&
           lhs.scale == rhs.scale &&
           lhs.position == rhs.position &&
           lhs.value == rhs.value &&
           lhs.nurbEnergy == rhs.nurbEnergy &&
           lhs.nurbMatter == rhs.nurbMatter &&
           lhs.potential == rhs.potential &&
           lhs.observable == rhs.observable &&
           lhs.spinEnergy == rhs.spinEnergy &&
           lhs.momentumEnergy == rhs.momentumEnergy &&
           lhs.fieldEnergy == rhs.fieldEnergy &&
           lhs.GodWaveEnergy == rhs.GodWaveEnergy;
}

struct EnergyResult {
    long double observable = 0.0L;
    long double potential = 0.0L;
    long double nurbMatter = 0.0L;
    long double nurbEnergy = 0.0L;
    long double spinEnergy = 0.0L;
    long double momentumEnergy = 0.0L;
    long double fieldEnergy = 0.0L;
    long double GodWaveEnergy = 0.0L;

    std::string toString() const {
        return std::format("Observable: {:.6Lf}, Potential: {:.6Lf}, NurbMatter: {:.6Lf}, NurbEnergy: {:.6Lf}, "
                           "SpinEnergy: {:.6Lf}, MomentumEnergy: {:.6Lf}, FieldEnergy: {:.6Lf}, GodWaveEnergy: {:.6Lf}",
                           observable, potential, nurbMatter, nurbEnergy,
                           spinEnergy, momentumEnergy, fieldEnergy, GodWaveEnergy);
    }
};

struct DimensionInteraction {
    int index;
    long double distance;
    long double strength;
    std::vector<long double> vectorPotential;
    long double godWaveAmplitude;

    DimensionInteraction() : index(0), distance(0.0L), strength(0.0L), vectorPotential(3, 0.0L), godWaveAmplitude(0.0L) {}
    DimensionInteraction(int idx, long double dist, long double str, std::vector<long double> vecPot, long double gwAmp)
        : index(idx), distance(dist), strength(str), vectorPotential(std::move(vecPot)), godWaveAmplitude(gwAmp) {}

    std::string toString() const {
        std::string vp;
        if (vectorPotential.size() >= 3) {
            vp = std::format("({:.6Lf}, {:.6Lf}, {:.6Lf})", vectorPotential[0], vectorPotential[1], vectorPotential[2]);
        } else {
            vp = "Invalid vector size";
        }
        return std::format("Index: {}, Distance: {:.6Lf}, Strength: {:.6Lf}, VectorPotential: {}, GodWaveAmplitude: {:.6Lf}",
                           index, distance, strength, vp, godWaveAmplitude);
    }
};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    int mode;
};

class DimensionalNavigator;
class UniversalEquation;
class AMOURANTH;

class UniversalEquation {
public:
    UniversalEquation(int maxDimensions, int mode, long double influence, long double weak, bool debug, uint64_t numVertices = 1);
    UniversalEquation(int maxDimensions, int mode, long double influence, long double weak, long double collapse,
                     long double twoD, long double threeDInfluence, long double oneDPermeation,
                     long double nurbMatterStrength, long double nurbEnergyStrength, long double alpha,
                     long double beta, long double carrollFactor, long double meanFieldApprox,
                     long double asymCollapse, long double perspectiveTrans, long double perspectiveFocal,
                     long double spinInteraction, long double emFieldStrength, long double renormFactor,
                     long double vacuumEnergy, long double godWaveFreq, bool debug, uint64_t numVertices);
    UniversalEquation(const UniversalEquation&) = delete;
    UniversalEquation& operator=(const UniversalEquation&) = delete;
    ~UniversalEquation();

    int getCurrentDimension() const;
    int getMode() const;
    bool getDebug() const;
    uint64_t getMaxVertices() const;
    int getMaxDimensions() const;
    long double getGodWaveFreq() const;
    long double getInfluence() const;
    long double getWeak() const;
    long double getCollapse() const;
    long double getTwoD() const;
    long double getThreeDInfluence() const;
    long double getOneDPermeation() const;
    long double getNurbMatterStrength() const;
    long double getNurbEnergyStrength() const;
    long double getAlpha() const;
    long double getBeta() const;
    long double getCarrollFactor() const;
    long double getMeanFieldApprox() const;
    long double getAsymCollapse() const;
    long double getPerspectiveTrans() const;
    long double getPerspectiveFocal() const;
    long double getSpinInteraction() const;
    long double getEMFieldStrength() const;
    long double getRenormFactor() const;
    long double getVacuumEnergy() const;
    bool getNeedsUpdate() const;
    long double getTotalCharge() const;
    long double getAvgProjScale() const;
    float getSimulationTime() const;
    long double getMaterialDensity() const;
    uint64_t getCurrentVertices() const;
    long double getOmega() const;
    long double getInvMaxDim() const;
    const std::vector<std::vector<long double>>& getNCubeVertices() const;
    const std::vector<std::vector<long double>>& getVertexMomenta() const;
    const std::vector<long double>& getVertexSpins() const;
    const std::vector<long double>& getVertexWaveAmplitudes() const;
    const std::vector<DimensionInteraction>& getInteractions() const;
    const std::vector<glm::vec3>& getProjectedVerts() const;
    const std::vector<long double>& getCachedCos() const;
    const std::vector<long double>& getNurbMatterControlPoints() const;
    const std::vector<long double>& getNurbEnergyControlPoints() const;
    const std::vector<long double>& getNurbKnots() const;
    const std::vector<long double>& getNurbWeights() const;
    const std::vector<DimensionData>& getDimensionData() const;
    DimensionalNavigator* getNavigator() const;
    const std::vector<long double>& getNCubeVertex(int vertexIndex) const;
    const std::vector<long double>& getVertexMomentum(int vertexIndex) const;
    long double getVertexSpin(int vertexIndex) const;
    long double getVertexWaveAmplitude(int vertexIndex) const;
    const glm::vec3& getProjectedVertex(int vertexIndex) const;

    void setCurrentDimension(int dimension);
    void setMode(int mode);
    void setInfluence(long double value);
    void setWeak(long double value);
    void setCollapse(long double value);
    void setTwoD(long double value);
    void setThreeDInfluence(long double value);
    void setOneDPermeation(long double value);
    void setNurbMatterStrength(long double value);
    void setNurbEnergyStrength(long double value);
    void setAlpha(long double value);
    void setBeta(long double value);
    void setCarrollFactor(long double value);
    void setMeanFieldApprox(long double value);
    void setAsymCollapse(long double value);
    void setPerspectiveTrans(long double value);
    void setPerspectiveFocal(long double value);
    void setSpinInteraction(long double value);
    void setEMFieldStrength(long double value);
    void setRenormFactor(long double value);
    void setVacuumEnergy(long double value);
    void setGodWaveFreq(long double value);
    void setDebug(bool value);
    void setCurrentVertices(uint64_t value);
    void setNavigator(DimensionalNavigator* nav);
    void setNCubeVertex(int vertexIndex, const std::vector<long double>& vertex);
    void setVertexMomentum(int vertexIndex, const std::vector<long double>& momentum);
    void setVertexSpin(int vertexIndex, long double spin);
    void setVertexWaveAmplitude(int vertexIndex, long double amplitude);
    void setProjectedVertex(int vertexIndex, const glm::vec3& vertex);
    void setNCubeVertices(const std::vector<std::vector<long double>>& vertices);
    void setVertexMomenta(const std::vector<std::vector<long double>>& momenta);
    void setVertexSpins(const std::vector<long double>& spins);
    void setVertexWaveAmplitudes(const std::vector<long double>& amplitudes);
    void setProjectedVertices(const std::vector<glm::vec3>& vertices);
    void setTotalCharge(long double value);
    void setMaterialDensity(long double density);

    void initializeNCube();
    void initializeWithRetry();
    void initializeCalculator(Camera* camera);
    void updateInteractions();
    EnergyResult compute();
    void evolveTimeStep(long double dt);
    void advanceCycle();
    std::vector<DimensionData> computeBatch(int startDim, int endDim);
    void exportToCSV(const std::string& filename, const std::vector<DimensionData>& data) const;
    DimensionData updateCache();
    long double computeGodWaveAmplitude(int vertexIndex, long double time) const;
    long double computeNurbMatter(int vertexIndex) const;
    long double computeNurbEnergy(int vertexIndex) const;
    long double computeSpinEnergy(int vertexIndex) const;
    long double computeEMField(int vertexIndex) const;
    long double computeGodWave(int vertexIndex) const;
    long double computeInteraction(int vertexIndex, long double distance) const;
    std::vector<long double> computeVectorPotential(int vertexIndex) const;
    long double computeGravitationalPotential(int vertexIndex, int otherIndex) const;
    std::vector<long double> computeGravitationalAcceleration(int vertexIndex) const;
    long double computeKineticEnergy(int vertexIndex) const;
    int findSpan(long double u, int degree, const std::vector<long double>& knots) const;
    std::vector<long double> basisFuncs(long double u, int span, int degree, const std::vector<long double>& knots) const;
    long double evaluateNURBS(long double u, const std::vector<long double>& controlPoints,
                              const std::vector<long double>& knots, const std::vector<long double>& weights,
                              int degree) const;
    void exportData() const;

    long double safeExp(long double x) const;
    long double safe_div(long double a, long double b) const;
    void validateVertexIndex(int vertexIndex, std::source_location loc = std::source_location::current()) const;
    void validateProjectedVertices() const;

    // Logging methods for important Universal Equation data
    void logDimensionData(int dimIndex) const;
    void logEnergyResult(const EnergyResult& result) const;
    void logInteraction(const DimensionInteraction& inter) const;
    void logInteractions() const;
    void logStatus() const;

private:
    long double influence_;
    long double weak_;
    long double collapse_;
    long double twoD_;
    long double threeDInfluence_;
    long double oneDPermeation_;
    long double nurbMatterStrength_;
    long double nurbEnergyStrength_;
    long double alpha_;
    long double beta_;
    long double carrollFactor_;
    long double meanFieldApprox_;
    long double asymCollapse_;
    long double perspectiveTrans_;
    long double perspectiveFocal_;
    long double spinInteraction_;
    long double emFieldStrength_;
    long double renormFactor_;
    long double vacuumEnergy_;
    long double godWaveFreq_;
    int currentDimension_;
    int mode_;
    bool debug_;
    bool needsUpdate_;
    long double totalCharge_;
    long double avgProjScale_;
    float simulationTime_;
    long double materialDensity_;
    uint64_t currentVertices_;
    const uint64_t maxVertices_;
    const int maxDimensions_;
    long double omega_;
    long double invMaxDim_;
    std::vector<std::vector<long double>> nCubeVertices_;
    std::vector<std::vector<long double>> vertexMomenta_;
    std::vector<long double> vertexSpins_;
    std::vector<long double> vertexWaveAmplitudes_;
    std::vector<DimensionInteraction> interactions_;
    std::vector<glm::vec3> projectedVerts_;
    std::vector<long double> cachedCos_;
    std::vector<long double> nurbMatterControlPoints_;
    std::vector<long double> nurbEnergyControlPoints_;
    std::vector<long double> nurbKnots_;
    std::vector<long double> nurbWeights_;
    std::vector<DimensionData> dimensionData_;
    DimensionalNavigator* navigator_;
};

inline void UniversalEquation::logDimensionData(int dimIndex) const {
    if (dimIndex < 0 || dimIndex >= static_cast<int>(dimensionData_.size())) {
        LOG_WARNING_CAT("UE", "Invalid dimension index: {}", dimIndex);
        return;
    }
    const auto& data = dimensionData_[dimIndex];
    LOG_INFO_CAT("UE", "Dimension {} initialized: scale={:.6Lf}, value={:.3f}, position={}", 
                 dimIndex, data.scale, data.value, data.position);
    LOG_INFO_CAT("UE", "Energies - NurbEnergy={:.6Lf}, NurbMatter={:.6Lf}, Potential={:.6Lf}, Observable={:.6Lf}", 
                 data.nurbEnergy, data.nurbMatter, data.potential, data.observable);
    LOG_INFO_CAT("UE", "Dynamic Energies - Spin={:.6Lf}, Momentum={:.6Lf}, Field={:.6Lf}, GodWave={:.6Lf}", 
                 data.spinEnergy, data.momentumEnergy, data.fieldEnergy, data.GodWaveEnergy);
}

inline void UniversalEquation::logEnergyResult(const EnergyResult& result) const {
    LOG_INFO_CAT("UE", "Energy Result: Observable={:.6Lf}, Potential={:.6Lf}, NurbMatter={:.6Lf}, NurbEnergy={:.6Lf}", 
                 result.observable, result.potential, result.nurbMatter, result.nurbEnergy);
    LOG_INFO_CAT("UE", "Advanced Energies: Spin={:.6Lf}, Momentum={:.6Lf}, Field={:.6Lf}, GodWave={:.6Lf}", 
                 result.spinEnergy, result.momentumEnergy, result.fieldEnergy, result.GodWaveEnergy);
}

inline void UniversalEquation::logInteraction(const DimensionInteraction& inter) const {
    LOG_INFO_CAT("UE", "Interaction Index {}: Distance={:.6Lf}, Strength={:.6Lf}, GodWaveAmp={:.6Lf}", 
                 inter.index, inter.distance, inter.strength, inter.godWaveAmplitude);
    if (inter.vectorPotential.size() >= 3) {
        LOG_INFO_CAT("UE", "Vector Potential: ({:.6Lf}, {:.6Lf}, {:.6Lf})", 
                     inter.vectorPotential[0], inter.vectorPotential[1], inter.vectorPotential[2]);
    }
}

inline void UniversalEquation::logInteractions() const {
    if (debug_) {
        LOG_INFO_CAT("UE", "Summary: {} interactions computed", interactions_.size());
        // Logging individual interactions is avoided to prevent overhead in loops
        // Instead, compute and log summary statistics
        long double avgDistance = 0.0L;
        long double avgStrength = 0.0L;
        if (!interactions_.empty()) {
            avgDistance = std::accumulate(interactions_.begin(), interactions_.end(), 0.0L,
                                          [](long double sum, const auto& i) { return sum + i.distance; }) / interactions_.size();
            avgStrength = std::accumulate(interactions_.begin(), interactions_.end(), 0.0L,
                                          [](long double sum, const auto& i) { return sum + i.strength; }) / interactions_.size();
        }
        LOG_INFO_CAT("UE", "Interaction Stats - Avg Distance: {:.6Lf}, Avg Strength: {:.6Lf}", avgDistance, avgStrength);
    }
}

inline void UniversalEquation::logStatus() const {
    LOG_INFO_CAT("UE", "UniversalEquation Status - Dimension: {}, Mode: {}, Influence: {:.4Lf}, Weak: {:.4Lf}, GodWaveFreq: {:.4Lf}",
                 currentDimension_, mode_, influence_, weak_, godWaveFreq_);
    if (!dimensionData_.empty()) {
        logDimensionData(0); // Log first dimension as example
    }
    logInteractions();
}

class DimensionalNavigator {
public:
    DimensionalNavigator(const char* name, int width, int height, VulkanRTX::VulkanRenderer& renderer);
    ~DimensionalNavigator() = default;

    Camera& getCamera();
    const Camera& getCamera() const;
    void setWidth(int width);
    void setHeight(int height);
    void setMode(int mode);
    void initialize(int dimension, uint64_t numVertices);
    int getWidth() const;
    int getHeight() const;
    int getMode() const;
    int getDimension() const;
    uint64_t getNumVertices() const;
    VulkanRTX::VulkanRenderer& getRenderer() const;

private:
    std::string name_;
    int width_;
    int height_;
    int mode_;
    int dimension_;
    uint64_t numVertices_;
    VulkanRTX::VulkanRenderer& renderer_;
    PerspectiveCamera camera_;
};

class UE {
public:
    UE();
    ~UE();
    void initializeDimensionData(VkDevice device, VkPhysicalDevice physicalDevice);
    void updateUBO(uint32_t currentFrame, const glm::mat4& view, const glm::mat4& proj, uint32_t mode);
    void cleanup(VkDevice device);

private:
    std::unique_ptr<UniversalEquation> universalEquation_;
    std::vector<DimensionData> dimensions_;
    std::vector<UniformBufferObject> ubos_;
    VkBuffer dimensionBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory dimensionBufferMemory_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
};

class AMOURANTH : public Camera {
public:
    AMOURANTH(DimensionalNavigator* navigator, VkDevice logicalDevice, VkDeviceMemory vertexMemory,
              VkDeviceMemory indexMemory, VkPipeline pipeline);
    ~AMOURANTH();

    glm::mat4 getViewMatrix() const override;
    glm::mat4 getProjectionMatrix() const override;
    int getMode() const override;
    glm::vec3 getPosition() const override;
    void setPosition(const glm::vec3& position) override;
    void setOrientation(float yaw, float pitch) override;
    void update(float deltaTime) override;
    void moveForward(float speed) override;
    void moveRight(float speed) override;
    void moveUp(float speed) override;
    void rotate(float yawDelta, float pitchDelta) override;
    void setFOV(float fov) override;
    float getFOV() const override;
    void setMode(int mode) override;
    void setModeWithLocation(int mode, std::source_location loc = std::source_location::current());
    const std::vector<glm::vec3>& getBalls() const;
    int getCurrentDimension() const;
    float getNurbMatter() const;
    float getNurbEnergy() const;
    UniversalEquation& getUniversalEquation();
    const UniversalEquation& getUniversalEquation() const;
    bool isPaused() const;
    const std::vector<DimensionData>& getCache() const;

    void setCurrentDimension(int dimension, std::source_location loc = std::source_location::current());
    void setNurbMatter(float matter, std::source_location loc = std::source_location::current());
    void setNurbEnergy(float energy, std::source_location loc = std::source_location::current());
    void adjustNurbMatter(float delta, std::source_location loc = std::source_location::current());
    void adjustNurbEnergy(float delta, std::source_location loc = std::source_location::current());
    void adjustInfluence(float delta, std::source_location loc = std::source_location::current());
    void updateZoom(bool zoomIn, std::source_location loc = std::source_location::current());
    void togglePause(std::source_location loc = std::source_location::current());
    void moveUserCam(float dx, float dy, float dz, std::source_location loc = std::source_location::current());
    void rotateCamera(float yaw, float pitch, std::source_location loc = std::source_location::current());
    void moveCamera(float dx, float dy, float dz, std::source_location loc = std::source_location::current());

    // Ray-tracing methods
    PFN_vkCmdTraceRaysKHR getVkCmdTraceRaysKHR() const { return vkCmdTraceRaysKHR_; }
    VkStridedDeviceAddressRegionKHR getRaygenSBT() const { return raygenSBT_; }
    VkStridedDeviceAddressRegionKHR getMissSBT() const { return missSBT_; }
    VkStridedDeviceAddressRegionKHR getHitSBT() const { return hitSBT_; }
    VkStridedDeviceAddressRegionKHR getCallableSBT() const { return callableSBT_; }

private:
    DimensionalNavigator* navigator_;
    VkDevice logicalDevice_;
    VkDeviceMemory vertexMemory_;
    VkDeviceMemory indexMemory_;
    VkPipeline pipeline_;
    int mode_;
    int currentDimension_;
    float nurbMatter_;
    float nurbEnergy_;
    std::unique_ptr<UniversalEquation> universalEquation_;
    glm::vec3 position_;
    glm::vec3 target_;
    glm::vec3 up_;
    float fov_;
    float aspectRatio_;
    float nearPlane_;
    float farPlane_;
    bool isPaused_;
    std::vector<glm::vec3> balls_;
    std::vector<DimensionData> cache_;

    // Ray-tracing members
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR_ = nullptr;
    VkStridedDeviceAddressRegionKHR raygenSBT_ = {};
    VkStridedDeviceAddressRegionKHR missSBT_ = {};
    VkStridedDeviceAddressRegionKHR hitSBT_ = {};
    VkStridedDeviceAddressRegionKHR callableSBT_ = {};
};

} // namespace UE

namespace std {
template<>
struct formatter<UE::AMOURANTH, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const UE::AMOURANTH& amouranth, FormatContext& ctx) const {
        return format_to(ctx.out(), "AMOURANTH(mode={}, dimension={}, nurbMatter={}, nurbEnergy={})",
                         amouranth.getMode(), amouranth.getCurrentDimension(),
                         amouranth.getNurbMatter(), amouranth.getNurbEnergy());
    }
};
} // namespace std

#endif // UE_INIT_HPP
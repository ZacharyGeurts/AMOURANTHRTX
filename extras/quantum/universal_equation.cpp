// universal_equation.cpp
// Core implementation of UniversalEquation for the AMOURANTH Console Engine.
// Manages N-dimensional calculations with NURBS-based dark matter and energy dynamics.
// Ensures equivalent contributions of dark matter (nurbMatter), dark energy (nurbEnergy),
// regular matter, kinetic energy, EM field, and gravitational potential, matching
// cosmological ratios (dark matter ~27%, dark energy ~68%, regular matter ~5%).
// Copyright Zachary Geurts 2025 (powered by Grok with Science B*! precision)

#include "ue_init.hpp"
#include <numbers>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <format>
#include <source_location>
#include <omp.h>

// Forward declaration of printDouble, defined in main.cpp
std::ostream& printDouble(std::ostream& os, double val, int precision = 6);

UniversalEquation::UniversalEquation(
    int maxDimensions,
    int mode,
    long double influence,
    long double weak,
    long double collapse,
    long double twoD,
    long double threeDInfluence,
    long double oneDPermeation,
    long double nurbMatterStrength,
    long double nurbEnergyStrength,
    long double nurbRegularMatterStrength,
    long double alpha,
    long double beta,
    long double carrollFactor,
    long double meanFieldApprox,
    long double asymCollapse,
    long double perspectiveTrans,
    long double perspectiveFocal,
    long double spinInteraction,
    long double emFieldStrength,
    long double renormFactor,
    long double vacuumEnergy,
    long double godWaveFreq,
    bool debug,
    uint64_t numVertices
) : influence_(std::clamp(influence, 0.0L, 10.0L)),
    weak_(std::clamp(weak, 0.0L, 1.0L)),
    collapse_(std::clamp(collapse, 0.0L, 5.0L)),
    twoD_(std::clamp(twoD, 0.0L, 5.0L)),
    threeDInfluence_(std::clamp(threeDInfluence, 0.0L, 5.0L)),
    oneDPermeation_(std::clamp(oneDPermeation, 0.0L, 5.0L)),
    nurbMatterStrength_(std::clamp(nurbMatterStrength, 0.0L, 1.0L)),
    nurbEnergyStrength_(std::clamp(nurbEnergyStrength, 0.0L, 2.0L)),
    nurbRegularMatterStrength_(std::clamp(nurbRegularMatterStrength, 0.0L, 1.0L)),
    alpha_(std::clamp(alpha, 0.01L, 10.0L)),
    beta_(std::clamp(beta, 0.0L, 1.0L)),
    carrollFactor_(std::clamp(carrollFactor, 0.0L, 1.0L)),
    meanFieldApprox_(std::clamp(meanFieldApprox, 0.0L, 1.0L)),
    asymCollapse_(std::clamp(asymCollapse, 0.0L, 1.0L)),
    perspectiveTrans_(std::clamp(perspectiveTrans, 0.0L, 10.0L)),
    perspectiveFocal_(std::clamp(perspectiveFocal, 1.0L, 20.0L)),
    spinInteraction_(std::clamp(spinInteraction, 0.0L, 1.0L)),
    emFieldStrength_(std::clamp(emFieldStrength, 0.0L, 1.0e7L)),
    renormFactor_(std::clamp(renormFactor, 0.1L, 10.0L)),
    vacuumEnergy_(std::clamp(vacuumEnergy, 0.0L, 1.0L)),
    godWaveFreq_(std::clamp(godWaveFreq, 0.1L, 10.0L)),
    currentDimension_(std::clamp(mode <= 0 ? 1 : mode, 1, maxDimensions <= 0 ? 9999 : maxDimensions)),
    mode_(std::clamp(mode <= 0 ? 1 : mode, 1, maxDimensions <= 0 ? 9999 : maxDimensions)),
    debug_(debug),
    needsUpdate_(true),
    totalCharge_(0.0L),
    avgProjScale_(1.0L),
    simulationTime_(0.0f),
    materialDensity_(1.0e6L),
    currentVertices_(0),
    maxVertices_(std::max<uint64_t>(1ULL, std::min(numVertices, static_cast<uint64_t>(1000000)))),
    maxDimensions_(std::max(1, std::min(maxDimensions <= 0 ? 9999 : maxDimensions, 9999))),
    omega_(maxDimensions_ > 0 ? 2.0L * std::numbers::pi_v<long double> / (2 * maxDimensions_ - 1) : 1.0L),
    invMaxDim_(maxDimensions_ > 0 ? 1.0L / maxDimensions_ : 1e-15L),
    nCubeVertices_(),
    vertexMomenta_(),
    vertexSpins_(),
    vertexWaveAmplitudes_(),
    interactions_(),
    cachedCos_(maxDimensions_ + 1, 0.0L),
    nurbMatterControlPoints_({0.27L, 0.27L, 0.27L, 0.27L, 0.27L}),
    nurbEnergyControlPoints_({0.68L, 0.68L, 0.68L, 0.68L, 0.68L}),
    nurbRegularMatterControlPoints_({0.05L, 0.05L, 0.05L, 0.05L, 0.05L}),
    nurbKineticControlPoints_({0.1L, 0.2L, 0.3L, 0.2L, 0.1L}),
    nurbEMControlPoints_({0.01L, 0.02L, 0.03L, 0.02L, 0.01L}),
    nurbPotentialControlPoints_({1.0L, 0.8L, 0.6L, 0.4L, 0.2L}),
    nurbKnots_({0.0L, 0.0L, 0.0L, 0.0L, 0.5L, 1.0L, 1.0L, 1.0L, 1.0L}),
    nurbWeights_({1.0L, 1.0L, 1.0L, 1.0L, 1.0L}),
    dimensionData_(std::vector<UE::DimensionData>(std::max(1, maxDimensions_), UE::DimensionData{
        0, 1.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L})) {
    LOG_INFO_CAT("Simulation", "Constructing UniversalEquation: maxVertices={}, maxDimensions={}, mode={}, godWaveFreq={:.6f}",
                 maxVertices_, maxDimensions_, mode_, godWaveFreq_);
    if (maxVertices_ > 1'000'000) {
        LOG_WARNING_CAT("Simulation", "High vertex count ({}) may cause memory issues", maxVertices_);
    }
    if (mode <= 0 || maxDimensions <= 0) {
        LOG_ERROR_CAT("Simulation", "maxDimensions and mode must be greater than 0: maxDimensions={}, mode={}", maxDimensions, mode);
        throw std::invalid_argument("maxDimensions and mode must be greater than 0");
    }
    if (debug_ && (influence != influence_ || weak != weak_ || collapse != collapse_ ||
                   twoD != twoD_ || threeDInfluence != threeDInfluence_ ||
                   oneDPermeation != oneDPermeation_ || nurbMatterStrength != nurbMatterStrength_ ||
                   nurbEnergyStrength != nurbEnergyStrength_ || nurbRegularMatterStrength != nurbRegularMatterStrength_ ||
                   alpha != alpha_ || beta != beta_ || carrollFactor != carrollFactor_ ||
                   meanFieldApprox != meanFieldApprox_ || asymCollapse != asymCollapse_ ||
                   perspectiveTrans != perspectiveTrans_ || perspectiveFocal != perspectiveFocal_ ||
                   spinInteraction != spinInteraction_ || emFieldStrength != emFieldStrength_ ||
                   renormFactor != renormFactor_ || vacuumEnergy != vacuumEnergy_ ||
                   godWaveFreq != godWaveFreq_)) {
        LOG_WARNING_CAT("Simulation", "Some input parameters were clamped to valid ranges");
    }
    try {
        initializeWithRetry();
        LOG_INFO_CAT("Simulation", "UniversalEquation initialized: vertices={}, totalCharge={:.6f}",
                     nCubeVertices_.size(), totalCharge_);
    } catch (const std::exception& e) {
        LOG_ERROR_CAT("Simulation", "Constructor failed: {}", e.what());
        throw;
    }
}

UniversalEquation::UniversalEquation(
    int maxDimensions,
    int mode,
    long double influence,
    long double weak,
    bool debug,
    uint64_t numVertices
) : UniversalEquation(
        maxDimensions, mode, influence, weak, 5.0L, 1.5L, 5.0L, 1.0L, 0.27L, 0.68L, 0.05L,
        0.01L, 0.5L, 0.1L, 0.5L, 0.5L, 2.0L, 4.0L, 1.0L, 1.0e6L, 1.0L, 0.5L, 2.0L, debug, numVertices) {
    LOG_DEBUG_CAT(debug_, "Simulation", "Initialized UniversalEquation with simplified constructor, godWaveFreq={:.6f}", godWaveFreq_);
}

UniversalEquation::UniversalEquation(const UniversalEquation& other)
    : influence_(other.influence_),
      weak_(other.weak_),
      collapse_(other.collapse_),
      twoD_(other.twoD_),
      threeDInfluence_(other.threeDInfluence_),
      oneDPermeation_(other.oneDPermeation_),
      nurbMatterStrength_(other.nurbMatterStrength_),
      nurbEnergyStrength_(other.nurbEnergyStrength_),
      nurbRegularMatterStrength_(other.nurbRegularMatterStrength_),
      alpha_(other.alpha_),
      beta_(other.beta_),
      carrollFactor_(other.carrollFactor_),
      meanFieldApprox_(other.meanFieldApprox_),
      asymCollapse_(other.asymCollapse_),
      perspectiveTrans_(other.perspectiveTrans_),
      perspectiveFocal_(other.perspectiveFocal_),
      spinInteraction_(other.spinInteraction_),
      emFieldStrength_(other.emFieldStrength_),
      renormFactor_(other.renormFactor_),
      vacuumEnergy_(other.vacuumEnergy_),
      godWaveFreq_(other.godWaveFreq_),
      currentDimension_(other.currentDimension_),
      mode_(other.mode_),
      debug_(other.debug_),
      needsUpdate_(other.needsUpdate_),
      totalCharge_(other.totalCharge_),
      avgProjScale_(other.avgProjScale_),
      simulationTime_(other.simulationTime_),
      materialDensity_(other.materialDensity_),
      currentVertices_(other.currentVertices_),
      maxVertices_(other.maxVertices_),
      maxDimensions_(other.maxDimensions_),
      omega_(other.omega_),
      invMaxDim_(other.invMaxDim_),
      nCubeVertices_(other.nCubeVertices_),
      vertexMomenta_(other.vertexMomenta_),
      vertexSpins_(other.vertexSpins_),
      vertexWaveAmplitudes_(other.vertexWaveAmplitudes_),
      interactions_(other.interactions_),
      cachedCos_(other.cachedCos_),
      nurbMatterControlPoints_(other.nurbMatterControlPoints_),
      nurbEnergyControlPoints_(other.nurbEnergyControlPoints_),
      nurbRegularMatterControlPoints_(other.nurbRegularMatterControlPoints_),
      nurbKineticControlPoints_(other.nurbKineticControlPoints_),
      nurbEMControlPoints_(other.nurbEMControlPoints_),
      nurbPotentialControlPoints_(other.nurbPotentialControlPoints_),
      nurbKnots_(other.nurbKnots_),
      nurbWeights_(other.nurbWeights_),
      dimensionData_(other.dimensionData_) {
    LOG_INFO_CAT("Simulation", "Copy constructing UniversalEquation: vertices={}", nCubeVertices_.size());
    try {
        initializeWithRetry();
        LOG_DEBUG_CAT(debug_, "Simulation", "Copy constructor completed: vertices={}", nCubeVertices_.size());
    } catch (const std::exception& e) {
        LOG_ERROR_CAT("Simulation", "Copy constructor failed: {}", e.what());
        throw;
    }
}

UniversalEquation& UniversalEquation::operator=(const UniversalEquation& other) {
    if (this != &other) {
        LOG_INFO_CAT("Simulation", "Assigning UniversalEquation: vertices={}", other.nCubeVertices_.size());
        influence_ = other.influence_;
        weak_ = other.weak_;
        collapse_ = other.collapse_;
        twoD_ = other.twoD_;
        threeDInfluence_ = other.threeDInfluence_;
        oneDPermeation_ = other.oneDPermeation_;
        nurbMatterStrength_ = other.nurbMatterStrength_;
        nurbEnergyStrength_ = other.nurbEnergyStrength_;
        nurbRegularMatterStrength_ = other.nurbRegularMatterStrength_;
        alpha_ = other.alpha_;
        beta_ = other.beta_;
        carrollFactor_ = other.carrollFactor_;
        meanFieldApprox_ = other.meanFieldApprox_;
        asymCollapse_ = other.asymCollapse_;
        perspectiveTrans_ = other.perspectiveTrans_;
        perspectiveFocal_ = other.perspectiveFocal_;
        spinInteraction_ = other.spinInteraction_;
        emFieldStrength_ = other.emFieldStrength_;
        renormFactor_ = other.renormFactor_;
        vacuumEnergy_ = other.vacuumEnergy_;
        godWaveFreq_ = other.godWaveFreq_;
        currentDimension_ = other.currentDimension_;
        mode_ = other.mode_;
        debug_ = other.debug_;
        needsUpdate_ = other.needsUpdate_;
        totalCharge_ = other.totalCharge_;
        avgProjScale_ = other.avgProjScale_;
        simulationTime_ = other.simulationTime_;
        materialDensity_ = other.materialDensity_;
        currentVertices_ = other.currentVertices_;
        nCubeVertices_ = other.nCubeVertices_;
        vertexMomenta_ = other.vertexMomenta_;
        vertexSpins_ = other.vertexSpins_;
        vertexWaveAmplitudes_ = other.vertexWaveAmplitudes_;
        interactions_ = other.interactions_;
        cachedCos_ = other.cachedCos_;
        nurbMatterControlPoints_ = other.nurbMatterControlPoints_;
        nurbEnergyControlPoints_ = other.nurbEnergyControlPoints_;
        nurbRegularMatterControlPoints_ = other.nurbRegularMatterControlPoints_;
        nurbKineticControlPoints_ = other.nurbKineticControlPoints_;
        nurbEMControlPoints_ = other.nurbEMControlPoints_;
        nurbPotentialControlPoints_ = other.nurbPotentialControlPoints_;
        nurbKnots_ = other.nurbKnots_;
        nurbWeights_ = other.nurbWeights_;
        dimensionData_ = other.dimensionData_;
        // Note: maxVertices_, maxDimensions_, omega_, and invMaxDim_ are const and not assigned
        try {
            initializeWithRetry();
            LOG_DEBUG_CAT(debug_, "Simulation", "Copy assignment completed: vertices={}", nCubeVertices_.size());
        } catch (const std::exception& e) {
            LOG_ERROR_CAT("Simulation", "Copy assignment failed: {}", e.what());
            throw;
        }
    }
    return *this;
}

UniversalEquation::~UniversalEquation() {
    LOG_DEBUG_CAT(debug_, "Simulation", "Destroying UniversalEquation: vertices={}", nCubeVertices_.size());
}

void UniversalEquation::initializeNCube() {
    try {
        LOG_INFO_CAT("Simulation", "Initializing n-cube: maxVertices={}, currentDimension={}", maxVertices_, currentDimension_);
        if (debug_ && maxVertices_ > 1'000'000) {
            LOG_WARNING_CAT("Simulation", "High vertex count ({}) may impact memory usage", maxVertices_);
        }

        nCubeVertices_.clear();
        vertexMomenta_.clear();
        vertexSpins_.clear();
        vertexWaveAmplitudes_.clear();
        interactions_.clear();
        LOG_DEBUG_CAT(debug_, "Simulation", "Cleared all vectors");

        nCubeVertices_.reserve(maxVertices_);
        vertexMomenta_.reserve(maxVertices_);
        vertexSpins_.reserve(maxVertices_);
        vertexWaveAmplitudes_.reserve(maxVertices_);
        interactions_.reserve(maxVertices_);
        if (nCubeVertices_.capacity() < maxVertices_) {
            LOG_ERROR_CAT("Simulation", "Failed to reserve {} elements for nCubeVertices_, actual capacity={}", maxVertices_, nCubeVertices_.capacity());
            throw std::bad_alloc();
        }
        LOG_DEBUG_CAT(debug_, "Simulation", "Reserved memory: nCubeVertices_.capacity()={}", nCubeVertices_.capacity());
        totalCharge_ = 0.0L;

        for (uint64_t i = 0; i < maxVertices_; ++i) {
            std::vector<long double> vertex(currentDimension_, 0.0L);
            std::vector<long double> momentum(currentDimension_, 0.0L);
            for (int j = 0; j < currentDimension_; ++j) {
                vertex[j] = (static_cast<long double>(i) / maxVertices_) * 0.0254L * currentDimension_;
                momentum[j] = (static_cast<long double>(i % 2) - 0.5L) * 0.01L * currentDimension_;
                if (std::isnan(vertex[j]) || std::isinf(vertex[j]) || std::isnan(momentum[j]) || std::isinf(momentum[j])) {
                    LOG_ERROR_CAT("Simulation", "Invalid vertex/momentum at vertex {}, dim {}: vertex={:.6f}, momentum={:.6f}",
                                  i, j, vertex[j], momentum[j]);
                    throw std::runtime_error("Invalid vertex/momentum value");
                }
            }
            long double spin = (i % 2 == 0 ? 0.032774L : -0.032774L) * currentDimension_;
            long double amplitude = oneDPermeation_ * (1.0L + 0.1L * (i / static_cast<long double>(maxVertices_))) * 0.1L;
            if (std::isnan(spin) || std::isinf(spin) || std::isnan(amplitude) || std::isinf(amplitude)) {
                LOG_ERROR_CAT("Simulation", "Invalid spin/amplitude at vertex {}: spin={:.6f}, amplitude={:.6f}",
                              i, spin, amplitude);
                throw std::runtime_error("Invalid spin/amplitude value");
            }

            nCubeVertices_.push_back(std::move(vertex));
            vertexMomenta_.push_back(std::move(momentum));
            vertexSpins_.push_back(spin);
            vertexWaveAmplitudes_.push_back(amplitude);
            interactions_.push_back(UE::DimensionInteraction(
                static_cast<int>(i), 0.0L, 0.0L, std::vector<long double>(std::min(3, currentDimension_), 0.0L), 0.0L));
            totalCharge_ += 1.0L / std::max(1.0L, static_cast<long double>(maxVertices_));

            if (nCubeVertices_.size() != i + 1 || vertexMomenta_.size() != i + 1 ||
                vertexSpins_.size() != i + 1 || vertexWaveAmplitudes_.size() != i + 1 ||
                interactions_.size() != i + 1) {
                LOG_ERROR_CAT("Simulation", "Size mismatch after push_back at vertex {}: nCubeVertices_={}, vertexMomenta_={}, vertexSpins_={}, vertexWaveAmplitudes_={}, interactions_={}",
                              i, nCubeVertices_.size(), vertexMomenta_.size(), vertexSpins_.size(), vertexWaveAmplitudes_.size(), interactions_.size());
                throw std::runtime_error("Size mismatch in initialization");
            }

            if (debug_ && (i >= 900 || i % 100 == 0 || i == maxVertices_ - 1)) {
                LOG_INFO_CAT("Simulation", "Initialized vertex {}: vertex[0]={:.6f}, momentum[0]={:.6f}, spin={:.6f}, amplitude={:.6f}",
                             i, nCubeVertices_[i][0], vertexMomenta_[i][0], vertexSpins_[i], vertexWaveAmplitudes_[i]);
            }
        }

        currentVertices_ = maxVertices_;
        LOG_INFO_CAT("Simulation", "n-cube initialized: vertices={}, totalCharge={:.6f}", nCubeVertices_.size(), totalCharge_);
    } catch (const std::exception& e) {
        LOG_ERROR_CAT("Simulation", "initializeNCube failed: {}", e.what());
        throw;
    }
}

void UniversalEquation::initializeWithRetry() {
    int attempts = 0;
    const int maxAttempts = 5;
    uint64_t currentVertices = maxVertices_;
    while (currentDimension_ >= 1 && attempts < maxAttempts) {
        try {
            if (nCubeVertices_.size() > currentVertices) {
                nCubeVertices_.resize(currentVertices);
                vertexMomenta_.resize(currentVertices);
                vertexSpins_.resize(currentVertices);
                vertexWaveAmplitudes_.resize(currentVertices);
                interactions_.resize(currentVertices, UE::DimensionInteraction(0, 0.0L, 0.0L, std::vector<long double>(std::min(3, currentDimension_), 0.0L), 0.0L));
            }
            initializeNCube();
            cachedCos_.resize(maxDimensions_ + 1);
            for (int i = 0; i <= maxDimensions_; ++i) {
                cachedCos_[i] = std::cos(omega_ * i);
            }
            updateInteractions();
            LOG_INFO_CAT("Simulation", "Initialization completed successfully for dimension {}", currentDimension_);
            return;
        } catch (const std::bad_alloc& e) {
            LOG_WARNING_CAT("Simulation", "Memory allocation failed for dimension {}. Reducing vertices to {}. Attempt {}/{}",
                            currentDimension_, currentVertices / 4, attempts + 1, maxAttempts);
            currentVertices = std::max<uint64_t>(1ULL, currentVertices / 4);
            needsUpdate_ = true;
            ++attempts;
        }
    }
    LOG_ERROR_CAT("Simulation", "Max retry attempts reached for initialization");
    throw std::runtime_error("Max retry attempts reached for initialization");
}

void UniversalEquation::updateInteractions() {
    LOG_INFO_CAT("Simulation", "Starting interaction update: vertices={}, dimension={}", nCubeVertices_.size(), currentDimension_);
    interactions_.clear();
    LOG_DEBUG_CAT(debug_, "Simulation", "Cleared interactions_");

    size_t d = static_cast<size_t>(currentDimension_);
    uint64_t numVertices = std::min(static_cast<uint64_t>(nCubeVertices_.size()), maxVertices_);
    if (debug_) {
        LOG_DEBUG_CAT(debug_, "Simulation", "Processing {} vertices (maxVertices_={})", numVertices, maxVertices_);
    }

    std::vector<long double> referenceVertex(d, 0.0L);
    std::vector<long double> sums(d, 0.0L);
    for (size_t i = 0; i < numVertices; ++i) {
        validateVertexIndex(static_cast<int>(i));
        for (size_t j = 0; j < d; ++j) {
            sums[j] += nCubeVertices_[i][j];
        }
    }
    for (size_t j = 0; j < d; ++j) {
        referenceVertex[j] = safe_div(sums[j], static_cast<long double>(numVertices));
    }

    interactions_.resize(numVertices);
    #pragma omp parallel for schedule(static)
    for (uint64_t i = 0; i < numVertices; ++i) {
        validateVertexIndex(static_cast<int>(i));
        const auto& v = nCubeVertices_[i];
        long double distance = 0.0L;
        for (size_t j = 0; j < d; ++j) {
            long double diff = v[j] - referenceVertex[j];
            distance += diff * diff;
        }
        distance = std::sqrt(std::max(distance, 1e-30L));
        long double strength = computeInteraction(static_cast<int>(i), distance);
        auto vecPot = computeVectorPotential(static_cast<int>(i));
        long double godWaveAmp = computeGodWaveAmplitude(static_cast<int>(i), simulationTime_);
        interactions_[i] = UE::DimensionInteraction(static_cast<int>(i), distance, strength, vecPot, godWaveAmp);
    }
    LOG_INFO_CAT("Simulation", "Interactions updated: interactions_.size()={}", interactions_.size());
}

UE::EnergyResult UniversalEquation::compute() {
    LOG_INFO_CAT("Simulation", "Starting compute: vertices={}, dimension={}", nCubeVertices_.size(), currentDimension_);
    if (needsUpdate_) {
        updateInteractions();
        needsUpdate_ = false;
    }

    UE::EnergyResult result{0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L};
    uint64_t numVertices = std::min(static_cast<uint64_t>(nCubeVertices_.size()), maxVertices_);

    if (nCubeVertices_.size() != numVertices || vertexMomenta_.size() != numVertices ||
        vertexSpins_.size() != numVertices || vertexWaveAmplitudes_.size() != numVertices) {
        LOG_ERROR_CAT("Simulation", "Vector size mismatch: nCubeVertices_={}, vertexMomenta_={}, vertexSpins_={}, vertexWaveAmplitudes_={}",
                      nCubeVertices_.size(), vertexMomenta_.size(), vertexSpins_.size(), vertexWaveAmplitudes_.size());
        throw std::runtime_error("Vector size mismatch in compute");
    }

    std::vector<long double> potentials(numVertices, 0.0L);
    std::vector<long double> nurbMatters(numVertices, 0.0L);
    std::vector<long double> nurbEnergies(numVertices, 0.0L);
    std::vector<long double> nurbRegularMatters(numVertices, 0.0L);
    std::vector<long double> spinEnergies(numVertices, 0.0L);
    std::vector<long double> momentumEnergies(numVertices, 0.0L);
    std::vector<long double> fieldEnergies(numVertices, 0.0L);
    std::vector<long double> godWaveEnergies(numVertices, 0.0L);

    long double nurbMatterSum = 0.0L;
    long double nurbEnergySum = 0.0L;
    long double nurbRegularMatterSum = 0.0L;
    long double potentialSum = 0.0L;
    long double spinEnergySum = 0.0L;
    long double momentumEnergySum = 0.0L;
    long double fieldEnergySum = 0.0L;
    long double godWaveEnergySum = 0.0L;
    const uint64_t chunkSize = std::max<uint64_t>(1, numVertices / (2 * omp_get_max_threads()));
    const long double minValue = 1e-30L; // Minimum non-zero value to prevent underflow

    #pragma omp parallel
    {
        long double thread_nurbMatterSum = 0.0L;
        long double thread_nurbEnergySum = 0.0L;
        long double thread_nurbRegularMatterSum = 0.0L;
        long double thread_potentialSum = 0.0L;
        long double thread_spinEnergySum = 0.0L;
        long double thread_momentumEnergySum = 0.0L;
        long double thread_fieldEnergySum = 0.0L;
        long double thread_godWaveEnergySum = 0.0L;

        #pragma omp for schedule(static, chunkSize) nowait
        for (uint64_t i = 0; i < numVertices; ++i) {
            validateVertexIndex(static_cast<int>(i));
            if (debug_ && (i % 1000 == 0 || i == 0 || i == numVertices - 1)) {
                LOG_INFO_CAT("Simulation", "Processing vertex {}: amplitude={:.6f}, spin={:.6f}",
                             i, vertexWaveAmplitudes_[i], vertexSpins_[i]);
            }

            long double totalPotential = 0.0L;
            uint64_t sampleStep = std::max<uint64_t>(1, numVertices / 100);
            for (uint64_t k = 0; k < numVertices; k += sampleStep) {
                if (static_cast<int>(k) == static_cast<int>(i)) continue;
                if (k >= nCubeVertices_.size()) {
                    LOG_WARNING_CAT("Simulation", "Skipping out-of-bounds vertex pair ({}, {}): max size={}", i, k, nCubeVertices_.size());
                    continue;
                }
                try {
                    long double potential = computeGravitationalPotential(static_cast<int>(i), static_cast<int>(k));
                    if (std::isnan(potential) || std::isinf(potential)) {
                        LOG_WARNING_CAT("Simulation", "Invalid potential for vertex pair ({}, {}): value={}", i, k, potential);
                        continue;
                    }
                    totalPotential += potential;
                } catch (const std::out_of_range& e) {
                    LOG_WARNING_CAT("Simulation", "Skipping invalid vertex pair ({}, {}): {}", i, k, e.what());
                    continue;
                }
            }
            totalPotential = std::max(totalPotential * static_cast<long double>(sampleStep), minValue);
            potentials[i] = totalPotential;

            long double nurbMatter = std::max(computeNurbMatter(static_cast<int>(i)), minValue);
            nurbMatters[i] = nurbMatter;

            long double nurbEnergy = std::max(computeNurbEnergy(static_cast<int>(i)), minValue);
            nurbEnergies[i] = nurbEnergy;

            long double nurbRegularMatter = std::max(computeNurbRegularMatter(static_cast<int>(i)), minValue);
            nurbRegularMatters[i] = nurbRegularMatter;

            long double spinEnergy = computeSpinEnergy(static_cast<int>(i));
            spinEnergies[i] = std::max(spinEnergy, minValue);

            long double momentumEnergy = computeKineticEnergy(static_cast<int>(i));
            momentumEnergies[i] = std::max(momentumEnergy, minValue);

            long double fieldEnergy = computeEMField(static_cast<int>(i));
            fieldEnergies[i] = std::max(fieldEnergy, minValue);

            long double godWaveEnergy = computeGodWave(static_cast<int>(i));
            godWaveEnergies[i] = std::max(godWaveEnergy, minValue);

            thread_nurbMatterSum += nurbMatters[i];
            thread_nurbEnergySum += nurbEnergies[i];
            thread_nurbRegularMatterSum += nurbRegularMatters[i];
            thread_potentialSum += potentials[i];
            thread_spinEnergySum += spinEnergies[i];
            thread_momentumEnergySum += momentumEnergies[i];
            thread_fieldEnergySum += fieldEnergies[i];
            thread_godWaveEnergySum += godWaveEnergies[i];

            if (i % (numVertices / 10) == 0 || i == numVertices - 1) {
                LOG_INFO_CAT("Simulation", "Compute progress: vertex {}/{}", i, numVertices);
            }
        }

        #pragma omp critical
        {
            nurbMatterSum += thread_nurbMatterSum;
            nurbEnergySum += thread_nurbEnergySum;
            nurbRegularMatterSum += thread_nurbRegularMatterSum;
            potentialSum += thread_potentialSum;
            spinEnergySum += thread_spinEnergySum;
            momentumEnergySum += thread_momentumEnergySum;
            fieldEnergySum += thread_fieldEnergySum;
            godWaveEnergySum += thread_godWaveEnergySum;
            LOG_INFO_CAT("Simulation", "Thread sums: nurbMatter={:.6f}, nurbEnergy={:.6f}, nurbRegularMatter={:.6f}, potential={:.6f}, spin={:.6f}, momentum={:.6f}, field={:.6f}, godWave={:.6f}",
                         thread_nurbMatterSum, thread_nurbEnergySum, thread_nurbRegularMatterSum, thread_potentialSum, thread_spinEnergySum, thread_momentumEnergySum, thread_fieldEnergySum, thread_godWaveEnergySum);
        }
    }

    LOG_INFO_CAT("Simulation", "Main computation loop completed, starting normalization");

    long double totalEnergySum = std::abs(nurbMatterSum) + std::abs(nurbEnergySum) + std::abs(nurbRegularMatterSum) +
                                 std::abs(potentialSum) + std::abs(spinEnergySum) + std::abs(momentumEnergySum) +
                                 std::abs(fieldEnergySum) + std::abs(godWaveEnergySum);

    if (totalEnergySum <= 1e-15L) {
        LOG_WARNING_CAT("Simulation", "Total energy sum too small: {}, setting to minimum value", totalEnergySum);
        totalEnergySum = 1e-10L;
    }

    long double remainingFraction = 1.0L - 0.27L - 0.68L - 0.05L;
    if (remainingFraction < 0.0L) {
        LOG_WARNING_CAT("Simulation", "Negative remaining fraction: {}, clamping to 0", remainingFraction);
        remainingFraction = 0.0L;
    }
    long double otherFraction = remainingFraction > 0.0L ? remainingFraction / 5.0L : 0.02L;

    result.nurbMatter = std::max(safe_div(0.27L * totalEnergySum, static_cast<long double>(numVertices)), minValue);
    result.nurbEnergy = std::max(safe_div(0.68L * totalEnergySum, static_cast<long double>(numVertices)), minValue);
    result.nurbRegularMatter = std::max(safe_div(0.05L * totalEnergySum, static_cast<long double>(numVertices)), minValue);
    result.potential = std::max(safe_div(std::abs(potentialSum) * otherFraction, static_cast<long double>(numVertices)), minValue);
    result.spinEnergy = std::max(safe_div(std::abs(spinEnergySum) * otherFraction, static_cast<long double>(numVertices)), minValue);
    result.momentumEnergy = std::max(safe_div(std::abs(momentumEnergySum) * otherFraction, static_cast<long double>(numVertices)), minValue);
    result.fieldEnergy = std::max(safe_div(std::abs(fieldEnergySum) * otherFraction, static_cast<long double>(numVertices)), minValue);
    result.GodWaveEnergy = std::max(safe_div(std::abs(godWaveEnergySum) * otherFraction, static_cast<long double>(numVertices)), minValue);

    long double observable = result.nurbMatter + result.nurbEnergy + result.nurbRegularMatter +
                             result.potential + result.spinEnergy + result.momentumEnergy +
                             result.fieldEnergy + result.GodWaveEnergy;
    if (std::isnan(observable) || std::isinf(observable)) {
        LOG_ERROR_CAT("Simulation", "Invalid observable energy: {}, resetting result to minimum values", observable);
        result = UE::EnergyResult{minValue, minValue, minValue, minValue, minValue, minValue, minValue, minValue, minValue * 8};
    } else {
        result.observable = std::max(observable, minValue);
    }

    LOG_INFO_CAT("Simulation", "Compute completed: observable={:.6f}, potential={:.6f}, nurbMatter={:.6f}, nurbEnergy={:.6f}, nurbRegularMatter={:.6f}, spinEnergy={:.6f}, momentumEnergy={:.6f}, fieldEnergy={:.6f}, GodWaveEnergy={:.6f}",
                 result.observable, result.potential, result.nurbMatter, result.nurbEnergy, result.nurbRegularMatter,
                 result.spinEnergy, result.momentumEnergy, result.fieldEnergy, result.GodWaveEnergy);
    return result;
}

void UniversalEquation::initializeCalculator() {
    LOG_INFO_CAT("Simulation", "Initializing calculator");
    try {
        needsUpdate_ = true;
        initializeWithRetry();
        printVertexTable();
        printInteractionTable();
        printParameterTable();
        printNURBSTable();
    } catch (const std::exception& e) {
        LOG_ERROR_CAT("Simulation", "initializeCalculator failed: {}", e.what());
        throw;
    }
}

void UniversalEquation::setGodWaveFreq(long double value) {
    godWaveFreq_ = std::clamp(value, 0.1L, 10.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set godWaveFreq: value={:.6f}", godWaveFreq_);
}

long double UniversalEquation::computeNurbMatter(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    long double u = static_cast<long double>(vertexIndex) / std::max(1.0L, static_cast<long double>(maxVertices_ - 1));
    if (u > 1.0L - 1e-15L) u = 1.0L - 1e-15L;
    long double nurbValue = evaluateNURBS(u, nurbMatterControlPoints_, nurbKnots_, nurbWeights_, 3);
    long double amplitude = vertexWaveAmplitudes_[vertexIndex];
    long double result = nurbMatterStrength_ * nurbValue * amplitude * materialDensity_ * currentDimension_;
    if (std::isnan(result) || std::isinf(result)) {
        LOG_WARNING_CAT("Simulation", "Invalid nurbMatter for vertex {}: u={:.6f}, nurbValue={:.6f}, amplitude={:.6f}, result={:.6f}, resetting to min value",
                        vertexIndex, u, nurbValue, amplitude, result);
        result = 1e-30L;
    }
    if (vertexIndex >= 900 || debug_) {
        LOG_INFO_CAT("Simulation", "Computed NURB dark matter for vertex {}: u={:.6f}, nurbValue={:.6f}, amplitude={:.6f}, result={:.6f}",
                     vertexIndex, u, nurbValue, amplitude, result);
    }
    return result;
}

long double UniversalEquation::computeNurbEnergy(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    long double u = static_cast<long double>(vertexIndex) / std::max(1.0L, static_cast<long double>(maxVertices_ - 1));
    if (u > 1.0L - 1e-15L) u = 1.0L - 1e-15L;
    long double nurbValue = evaluateNURBS(u, nurbEnergyControlPoints_, nurbKnots_, nurbWeights_, 3);
    long double amplitude = vertexWaveAmplitudes_[vertexIndex];
    long double result = nurbEnergyStrength_ * nurbValue * amplitude * materialDensity_ * currentDimension_;
    if (std::isnan(result) || std::isinf(result)) {
        LOG_WARNING_CAT("Simulation", "Invalid nurbEnergy for vertex {}: u={:.6f}, nurbValue={:.6f}, amplitude={:.6f}, result={:.6f}, resetting to min value",
                        vertexIndex, u, nurbValue, amplitude, result);
        result = 1e-30L;
    }
    if (vertexIndex >= 900 || debug_) {
        LOG_INFO_CAT("Simulation", "Computed NURB dark energy for vertex {}: u={:.6f}, nurbValue={:.6f}, amplitude={:.6f}, result={:.6f}",
                     vertexIndex, u, nurbValue, amplitude, result);
    }
    return result;
}

long double UniversalEquation::computeNurbRegularMatter(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    long double u = static_cast<long double>(vertexIndex) / std::max(1.0L, static_cast<long double>(maxVertices_ - 1));
    if (u > 1.0L - 1e-15L) u = 1.0L - 1e-15L;
    long double nurbValue = evaluateNURBS(u, nurbRegularMatterControlPoints_, nurbKnots_, nurbWeights_, 3);
    long double amplitude = vertexWaveAmplitudes_[vertexIndex];
    long double result = nurbRegularMatterStrength_ * nurbValue * amplitude * materialDensity_ * currentDimension_;
    if (std::isnan(result) || std::isinf(result)) {
        LOG_WARNING_CAT("Simulation", "Invalid nurbRegularMatter for vertex {}: u={:.6f}, nurbValue={:.6f}, amplitude={:.6f}, result={:.6f}, resetting to min value",
                        vertexIndex, u, nurbValue, amplitude, result);
        result = 1e-30L;
    }
    if (vertexIndex >= 900 || debug_) {
        LOG_INFO_CAT("Simulation", "Computed NURB regular matter for vertex {}: u={:.6f}, nurbValue={:.6f}, amplitude={:.6f}, result={:.6f}",
                     vertexIndex, u, nurbValue, amplitude, result);
    }
    return result;
}

long double UniversalEquation::computeSpinEnergy(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    long double u = static_cast<long double>(vertexIndex) / std::max(1.0L, static_cast<long double>(maxVertices_ - 1));
    if (u > 1.0L - 1e-15L) u = 1.0L - 1e-15L;
    long double nurbValue = evaluateNURBS(u, nurbKineticControlPoints_, nurbKnots_, nurbWeights_, 3);
    long double result = spinInteraction_ * std::abs(vertexSpins_[vertexIndex]) * nurbValue * 0.2L * currentDimension_;
    if (std::isnan(result) || std::isinf(result)) {
        LOG_WARNING_CAT("Simulation", "Invalid spinEnergy for vertex {}: u={:.6f}, nurbValue={:.6f}, spin={:.6f}, result={:.6f}, resetting to min value",
                        vertexIndex, u, nurbValue, vertexSpins_[vertexIndex], result);
        result = 1e-30L;
    }
    if (vertexIndex >= 900 || debug_) {
        LOG_INFO_CAT("Simulation", "Computed NURB spin energy for vertex {}: u={:.6f}, nurbValue={:.6f}, result={:.6f}",
                     vertexIndex, u, nurbValue, result);
    }
    return result;
}

long double UniversalEquation::computeEMField(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    long double u = static_cast<long double>(vertexIndex) / std::max(1.0L, static_cast<long double>(maxVertices_ - 1));
    if (u > 1.0L - 1e-15L) u = 1.0L - 1e-15L;
    long double nurbValue = evaluateNURBS(u, nurbEMControlPoints_, nurbKnots_, nurbWeights_, 3);
    long double amplitude = vertexWaveAmplitudes_[vertexIndex];
    long double result = emFieldStrength_ * nurbValue * amplitude * currentDimension_;
    if (std::isnan(result) || std::isinf(result)) {
        LOG_WARNING_CAT("Simulation", "Invalid fieldEnergy for vertex {}: u={:.6f}, nurbValue={:.6f}, amplitude={:.6f}, result={:.6f}, resetting to min value",
                        vertexIndex, u, nurbValue, amplitude, result);
        result = 1e-30L;
    }
    if (vertexIndex >= 900 || debug_) {
        LOG_INFO_CAT("Simulation", "Computed NURB EM field for vertex {}: u={:.6f}, nurbValue={:.6f}, amplitude={:.6f}, result={:.6f}",
                     vertexIndex, u, nurbValue, amplitude, result);
    }
    return result;
}

long double UniversalEquation::computeGodWave(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    long double u = static_cast<long double>(vertexIndex) / std::max(1.0L, static_cast<long double>(maxVertices_ - 1));
    if (u > 1.0L - 1e-15L) u = 1.0L - 1e-15L;
    long double nurbValue = evaluateNURBS(u, nurbKineticControlPoints_, nurbKnots_, nurbWeights_, 3);
    long double amplitude = vertexWaveAmplitudes_[vertexIndex];
    long double result = godWaveFreq_ * nurbValue * amplitude * currentDimension_;
    if (std::isnan(result) || std::isinf(result)) {
        LOG_WARNING_CAT("Simulation", "Invalid godWaveEnergy for vertex {}: u={:.6f}, nurbValue={:.6f}, amplitude={:.6f}, result={:.6f}, resetting to min value",
                        vertexIndex, u, nurbValue, amplitude, result);
        result = 1e-30L;
    }
    if (vertexIndex >= 900 || debug_) {
        LOG_INFO_CAT("Simulation", "Computed NURB God wave for vertex {}: u={:.6f}, nurbValue={:.6f}, amplitude={:.6f}, result={:.6f}",
                     vertexIndex, u, nurbValue, amplitude, result);
    }
    return result;
}

long double UniversalEquation::computeInteraction(int vertexIndex, long double distance) const {
    validateVertexIndex(vertexIndex);
    long double result = influence_ * safe_div(1.0L, distance + 1e-15L) * currentDimension_;
    if (std::isnan(result) || std::isinf(result)) {
        LOG_WARNING_CAT("Simulation", "Invalid interaction for vertex {}: distance={:.6f}, result={:.6f}, resetting to min value",
                        vertexIndex, distance, result);
        result = 1e-30L;
    }
    if (vertexIndex >= 900 || debug_) {
        LOG_INFO_CAT("Simulation", "Computed interaction for vertex {}: distance={:.6f}, result={:.6f}",
                     vertexIndex, distance, result);
    }
    return result;
}

std::vector<long double> UniversalEquation::computeVectorPotential(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    std::vector<long double> result(std::min(3, currentDimension_), 0.0L);
    for (int i = 0; i < std::min(3, currentDimension_); ++i) {
        result[i] = vertexMomenta_[vertexIndex][i] * weak_ * currentDimension_;
        if (std::isnan(result[i]) || std::isinf(result[i])) {
            result[i] = 1e-30L;
        }
    }
    if (vertexIndex >= 900 || debug_) {
        LOG_INFO_CAT("Simulation", "Computed vector potential for vertex {}: result size={}", vertexIndex, result.size());
    }
    return result;
}

long double UniversalEquation::computeGravitationalPotential(int vertexIndex, int otherIndex) const {
    validateVertexIndex(vertexIndex);
    validateVertexIndex(otherIndex);
    if (vertexIndex == otherIndex) {
        return 0.0L;
    }
    long double u = static_cast<long double>(vertexIndex) / std::max(1.0L, static_cast<long double>(maxVertices_ - 1));
    if (u > 1.0L - 1e-15L) u = 1.0L - 1e-15L;
    long double nurbValue = evaluateNURBS(u, nurbPotentialControlPoints_, nurbKnots_, nurbWeights_, 3);
    long double distance = 0.0L;
    const auto& v1 = nCubeVertices_[vertexIndex];
    const auto& v2 = nCubeVertices_[otherIndex];
    for (size_t j = 0; j < static_cast<size_t>(currentDimension_); ++j) {
        long double diff = v1[j] - v2[j];
        distance += diff * diff;
    }
    distance = std::sqrt(std::max(distance, 1e-30L));
    long double result = influence_ * nurbValue * safe_div(1.0L, distance) * currentDimension_;
    if (std::isnan(result) || std::isinf(result)) {
        LOG_WARNING_CAT("Simulation", "Invalid gravitational potential for vertices {} and {}: u={:.6f}, nurbValue={:.6f}, result={:.6f}, resetting to min value",
                        vertexIndex, otherIndex, u, nurbValue, result);
        result = 1e-30L;
    }
    if (vertexIndex >= 900 || debug_) {
        LOG_INFO_CAT("Simulation", "Computed NURB gravitational potential for vertices {} and {}: u={:.6f}, nurbValue={:.6f}, result={:.6f}",
                     vertexIndex, otherIndex, u, nurbValue, result);
    }
    return result;
}

std::vector<long double> UniversalEquation::computeGravitationalAcceleration(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    std::vector<long double> acceleration(currentDimension_, 0.0L);
    for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
        if (static_cast<int>(i) == vertexIndex) continue;
        long double u = static_cast<long double>(vertexIndex) / std::max(1.0L, static_cast<long double>(maxVertices_ - 1));
        if (u > 1.0L - 1e-15L) u = 1.0L - 1e-15L;
        long double nurbValue = evaluateNURBS(u, nurbPotentialControlPoints_, nurbKnots_, nurbWeights_, 3);
        long double distance = 0.0L;
        const auto& v1 = nCubeVertices_[vertexIndex];
        const auto& v2 = nCubeVertices_[i];
        for (size_t j = 0; j < static_cast<size_t>(currentDimension_); ++j) {
            long double diff = v1[j] - v2[j];
            distance += diff * diff;
        }
        distance = std::sqrt(std::max(distance, 1e-30L));
        long double force = influence_ * nurbValue * safe_div(1.0L, distance * distance) * currentDimension_;
        for (size_t j = 0; j < static_cast<size_t>(currentDimension_); ++j) {
            acceleration[j] += force * (v2[j] - v1[j]) / distance;
        }
    }
    for (size_t j = 0; j < static_cast<size_t>(currentDimension_); ++j) {
        if (std::isnan(acceleration[j]) || std::isinf(acceleration[j])) {
            acceleration[j] = 1e-30L;
        }
    }
    if (vertexIndex >= 900 || debug_) {
        LOG_INFO_CAT("Simulation", "Computed NURB gravitational acceleration for vertex {}: result size={}", vertexIndex, acceleration.size());
    }
    return acceleration;
}

long double UniversalEquation::computeGodWaveAmplitude(int vertexIndex, long double time) const {
    validateVertexIndex(vertexIndex);
    long double u = static_cast<long double>(vertexIndex) / std::max(1.0L, static_cast<long double>(maxVertices_ - 1));
    if (u > 1.0L - 1e-15L) u = 1.0L - 1e-15L;
    long double nurbValue = evaluateNURBS(u, nurbKineticControlPoints_, nurbKnots_, nurbWeights_, 3);
    long double result = godWaveFreq_ * vertexWaveAmplitudes_[vertexIndex] * std::cos(godWaveFreq_ * time) * nurbValue * currentDimension_;
    if (std::isnan(result) || std::isinf(result)) {
        LOG_WARNING_CAT("Simulation", "Invalid godWaveAmplitude for vertex {}: time={:.6f}, nurbValue={:.6f}, result={:.6f}, resetting to min value",
                        vertexIndex, time, nurbValue, result);
        result = 1e-30L;
    }
    if (vertexIndex >= 900 || debug_) {
        LOG_INFO_CAT("Simulation", "Computed NURB God wave amplitude for vertex {} at time {:.6f}: nurbValue={:.6f}, result={:.6f}",
                     vertexIndex, time, nurbValue, result);
    }
    return result;
}

long double UniversalEquation::computeKineticEnergy(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    long double u = static_cast<long double>(vertexIndex) / std::max(1.0L, static_cast<long double>(maxVertices_ - 1));
    if (u > 1.0L - 1e-15L) u = 1.0L - 1e-15L;
    long double nurbValue = evaluateNURBS(u, nurbKineticControlPoints_, nurbKnots_, nurbWeights_, 3);
    long double kineticEnergy = 0.0L;
    const auto& momentum = vertexMomenta_[vertexIndex];
    for (size_t j = 0; j < static_cast<size_t>(currentDimension_); ++j) {
        kineticEnergy += momentum[j] * momentum[j];
    }
    long double result = nurbValue * 0.5L * materialDensity_ * kineticEnergy * currentDimension_;
    if (std::isnan(result) || std::isinf(result)) {
        LOG_WARNING_CAT("Simulation", "Invalid kineticEnergy for vertex {}: u={:.6f}, nurbValue={:.6f}, kineticEnergy={:.6f}, result={:.6f}, resetting to min value",
                        vertexIndex, u, nurbValue, kineticEnergy, result);
        result = 1e-30L;
    }
    if (vertexIndex >= 900 || debug_) {
        LOG_INFO_CAT("Simulation", "Computed NURB kinetic energy for vertex {}: u={:.6f}, nurbValue={:.6f}, kineticEnergy={:.6f}, result={:.6f}",
                     vertexIndex, u, nurbValue, kineticEnergy, result);
    }
    return result;
}

int UniversalEquation::findSpan(long double u, int degree, const std::vector<long double>& knots) const {
    if (u < knots[degree] || u > knots[knots.size() - degree - 1]) {
        LOG_WARNING_CAT("Simulation", "Parameter u={:.6f} out of range [{:.6f}, {:.6f}], clamping",
                        u, knots[degree], knots[knots.size() - degree - 1]);
        u = std::clamp(u, knots[degree], knots[knots.size() - degree - 1]);
    }
    int low = degree;
    int high = knots.size() - degree - 1;
    int mid = (low + high) / 2;
    while (u < knots[mid] || u >= knots[mid + 1]) {
        if (u < knots[mid]) {
            high = mid;
        } else {
            low = mid;
        }
        mid = (low + high) / 2;
    }
    return mid;
}

std::vector<long double> UniversalEquation::basisFuncs(long double u, int span, int degree, const std::vector<long double>& knots) const {
    std::vector<long double> N(degree + 1, 0.0L);
    std::vector<long double> left(degree + 1, 0.0L);
    std::vector<long double> right(degree + 1, 0.0L);
    N[0] = 1.0L;
    for (int j = 1; j <= degree; ++j) {
        left[j] = u - knots[span + 1 - j];
        right[j] = knots[span + j] - u;
        long double saved = 0.0L;
        for (int r = 0; r < j; ++r) {
            long double temp = safe_div(N[r], right[r + 1] + left[j - r]);
            N[r] = saved + right[r + 1] * temp;
            saved = left[j - r] * temp;
        }
        N[j] = saved;
    }
    return N;
}

long double UniversalEquation::evaluateNURBS(long double u, const std::vector<long double>& controlPoints,
                                            const std::vector<long double>& knots, const std::vector<long double>& weights,
                                            int degree) const {
    if (controlPoints.size() != weights.size() || controlPoints.size() + degree + 1 != knots.size()) {
        LOG_ERROR_CAT("Simulation", "NURBS parameter mismatch: controlPoints.size()={}, weights.size()={}, knots.size()={}, degree={}",
                      controlPoints.size(), weights.size(), knots.size(), degree);
        throw std::invalid_argument("NURBS parameter mismatch");
    }
    if (u > 1.0L - 1e-15L) u = 1.0L - 1e-15L;
    if (u < 0.0L) u = 0.0L;
    int span = findSpan(u, degree, knots);
    std::vector<long double> basis = basisFuncs(u, span, degree, knots);
    long double sum = 0.0L;
    long double weightSum = 0.0L;
    for (int i = 0; i <= degree; ++i) {
        int idx = span - degree + i;
        if (idx >= 0 && static_cast<size_t>(idx) < controlPoints.size()) {
            sum += basis[i] * controlPoints[idx] * weights[idx];
            weightSum += basis[i] * weights[idx];
        }
    }
    long double result = safe_div(sum, weightSum);
    if (std::isnan(result) || std::isinf(result)) {
        LOG_WARNING_CAT("Simulation", "Invalid NURBS evaluation: u={:.6f}, sum={:.6f}, weightSum={:.6f}, result={:.6f}, resetting to min value",
                        u, sum, weightSum, result);
        result = 1e-30L;
    }
    return result;
}

long double UniversalEquation::safeExp(long double x) const {
    if (std::isnan(x) || std::isinf(x)) {
        LOG_WARNING_CAT("Simulation", "Invalid exponent: x={:.6f}, returning 1.0", x);
        return 1.0L;
    }
    if (x > 100.0L) {
        LOG_WARNING_CAT("Simulation", "Large exponent: x={:.6f}, clamping to 100.0", x);
        x = 100.0L;
    } else if (x < -100.0L) {
        LOG_WARNING_CAT("Simulation", "Large negative exponent: x={:.6f}, clamping to -100.0", x);
        x = -100.0L;
    }
    return std::exp(x);
}

long double UniversalEquation::safe_div(long double a, long double b) const {
    if (std::abs(b) < 1e-30L || std::isnan(b) || std::isinf(b)) {
        LOG_WARNING_CAT("Simulation", "Invalid divisor: a={:.6f}, b={:.6f}, returning min value", a, b);
        return a >= 0 ? 1e-30L : -1e-30L;
    }
    long double result = a / b;
    if (std::isnan(result) || std::isinf(result)) {
        LOG_WARNING_CAT("Simulation", "Invalid division: a={:.6f}, b={:.6f}, result={:.6f}, returning min value",
                        a, b, result);
        return a >= 0 ? 1e-30L : -1e-30L;
    }
    return std::max(std::abs(result), 1e-30L) * (result >= 0 ? 1.0L : -1.0L);
}

void UniversalEquation::validateVertexIndex(int vertexIndex, const std::source_location& loc) const {
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= nCubeVertices_.size()) {
        LOG_ERROR_CAT("Simulation", "Invalid vertex index {} at {}:{}:{}",
                      vertexIndex, loc.file_name(), loc.line(), loc.column());
        throw std::out_of_range("Invalid vertex index");
    }
}

void UniversalEquation::setCurrentDimension(int dimension) {
    if (dimension < 1 || dimension > maxDimensions_) {
        LOG_ERROR_CAT("Simulation", "Invalid dimension: {}, valid range [1, {}]", dimension, maxDimensions_);
        throw std::invalid_argument("Invalid dimension");
    }
    currentDimension_ = dimension;
    currentVertices_ = std::min(maxVertices_, static_cast<uint64_t>(std::pow(2.0, std::min(20.0, static_cast<double>(dimension)))));
    initializeWithRetry();
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set currentDimension: value={}", dimension);
}

void UniversalEquation::setMode(int mode) {
    if (mode < 1 || mode > maxDimensions_) {
        LOG_ERROR_CAT("Simulation", "Invalid mode: {}, valid range [1, {}]", mode, maxDimensions_);
        throw std::invalid_argument("Invalid mode");
    }
    mode_ = mode;
    if (currentDimension_ > mode_) {
        currentDimension_ = mode_;
        currentVertices_ = std::min(maxVertices_, static_cast<uint64_t>(std::pow(2.0, std::min(20.0, static_cast<double>(currentDimension_)))));
        initializeWithRetry();
    }
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set mode: value={}", mode_);
}

void UniversalEquation::setInfluence(long double value) {
    influence_ = std::clamp(value, 0.0L, 10.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set influence: value={:.6f}", influence_);
}

void UniversalEquation::setWeak(long double value) {
    weak_ = std::clamp(value, 0.0L, 1.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set weak: value={:.6f}", weak_);
}

void UniversalEquation::setCollapse(long double value) {
    collapse_ = std::clamp(value, 0.0L, 5.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set collapse: value={:.6f}", collapse_);
}

void UniversalEquation::setTwoD(long double value) {
    twoD_ = std::clamp(value, 0.0L, 5.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set twoD: value={:.6f}", twoD_);
}

void UniversalEquation::setThreeDInfluence(long double value) {
    threeDInfluence_ = std::clamp(value, 0.0L, 5.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set threeDInfluence: value={:.6f}", threeDInfluence_);
}

void UniversalEquation::setOneDPermeation(long double value) {
    oneDPermeation_ = std::clamp(value, 0.0L, 5.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set oneDPermeation: value={:.6f}", oneDPermeation_);
}

void UniversalEquation::setNurbMatterStrength(long double value) {
    nurbMatterStrength_ = std::clamp(value, 0.0L, 1.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set nurbMatterStrength: value={:.6f}", nurbMatterStrength_);
}

void UniversalEquation::setNurbEnergyStrength(long double value) {
    nurbEnergyStrength_ = std::clamp(value, 0.0L, 2.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set nurbEnergyStrength: value={:.6f}", nurbEnergyStrength_);
}

void UniversalEquation::setNurbRegularMatterStrength(long double value) {
    nurbRegularMatterStrength_ = std::clamp(value, 0.0L, 1.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set nurbRegularMatterStrength: value={:.6f}", nurbRegularMatterStrength_);
}

void UniversalEquation::setAlpha(long double value) {
    alpha_ = std::clamp(value, 0.01L, 10.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set alpha: value={:.6f}", alpha_);
}

void UniversalEquation::setBeta(long double value) {
    beta_ = std::clamp(value, 0.0L, 1.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set beta: value={:.6f}", beta_);
}

void UniversalEquation::setCarrollFactor(long double value) {
    carrollFactor_ = std::clamp(value, 0.0L, 1.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set carrollFactor: value={:.6f}", carrollFactor_);
}

void UniversalEquation::setMeanFieldApprox(long double value) {
    meanFieldApprox_ = std::clamp(value, 0.0L, 1.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set meanFieldApprox: value={:.6f}", meanFieldApprox_);
}

void UniversalEquation::setAsymCollapse(long double value) {
    asymCollapse_ = std::clamp(value, 0.0L, 1.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set asymCollapse: value={:.6f}", asymCollapse_);
}

void UniversalEquation::setPerspectiveTrans(long double value) {
    perspectiveTrans_ = std::clamp(value, 0.0L, 10.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set perspectiveTrans: value={:.6f}", perspectiveTrans_);
}

void UniversalEquation::setPerspectiveFocal(long double value) {
    perspectiveFocal_ = std::clamp(value, 1.0L, 20.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set perspectiveFocal: value={:.6f}", perspectiveFocal_);
}

void UniversalEquation::setSpinInteraction(long double value) {
    spinInteraction_ = std::clamp(value, 0.0L, 1.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set spinInteraction: value={:.6f}", spinInteraction_);
}

void UniversalEquation::setEMFieldStrength(long double value) {
    emFieldStrength_ = std::clamp(value, 0.0L, 1.0e7L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set emFieldStrength: value={:.6f}", emFieldStrength_);
}

void UniversalEquation::setRenormFactor(long double value) {
    renormFactor_ = std::clamp(value, 0.1L, 10.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set renormFactor: value={:.6f}", renormFactor_);
}

void UniversalEquation::setVacuumEnergy(long double value) {
    vacuumEnergy_ = std::clamp(value, 0.0L, 1.0L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set vacuumEnergy: value={:.6f}", vacuumEnergy_);
}

void UniversalEquation::setDebug(bool value) {
    debug_ = value;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set debug: value={}", debug_);
}

void UniversalEquation::setCurrentVertices(uint64_t value) {
    if (value == 0 || value > maxVertices_) {
        LOG_ERROR_CAT("Simulation", "Invalid currentVertices: {}, valid range [1, {}]", value, maxVertices_);
        throw std::invalid_argument("Invalid currentVertices");
    }
    currentVertices_ = value;
    initializeWithRetry();
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set currentVertices: value={}", currentVertices_);
}

void UniversalEquation::setNCubeVertex(int vertexIndex, const std::vector<long double>& vertex) {
    validateVertexIndex(vertexIndex);
    if (vertex.size() != static_cast<size_t>(currentDimension_)) {
        LOG_ERROR_CAT("Simulation", "Vertex dimension mismatch at index {}: expected size={}, actual size={}",
                      vertexIndex, currentDimension_, vertex.size());
        throw std::invalid_argument("Vertex dimension mismatch");
    }
    nCubeVertices_[vertexIndex] = vertex;
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set nCubeVertex at index {}: vertex[0]={:.6f}", vertexIndex, vertex[0]);
}

void UniversalEquation::setVertexMomentum(int vertexIndex, const std::vector<long double>& momentum) {
    validateVertexIndex(vertexIndex);
    if (momentum.size() != static_cast<size_t>(currentDimension_)) {
        LOG_ERROR_CAT("Simulation", "Momentum dimension mismatch at index {}: expected size={}, actual size={}",
                      vertexIndex, currentDimension_, momentum.size());
        throw std::invalid_argument("Momentum dimension mismatch");
    }
    vertexMomenta_[vertexIndex] = momentum;
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set vertexMomentum at index {}: momentum[0]={:.6f}", vertexIndex, momentum[0]);
}

void UniversalEquation::setVertexSpin(int vertexIndex, long double spin) {
    validateVertexIndex(vertexIndex);
    vertexSpins_[vertexIndex] = spin;
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set vertexSpin at index {}: spin={:.6f}", vertexIndex, spin);
}

void UniversalEquation::setVertexWaveAmplitude(int vertexIndex, long double amplitude) {
    validateVertexIndex(vertexIndex);
    vertexWaveAmplitudes_[vertexIndex] = amplitude;
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set vertexWaveAmplitude at index {}: amplitude={:.6f}", vertexIndex, amplitude);
}

void UniversalEquation::setNCubeVertices(const std::vector<std::vector<long double>>& vertices) {
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].size() != static_cast<size_t>(currentDimension_)) {
            LOG_ERROR_CAT("Simulation", "Vertex dimension mismatch at index {}: expected size={}, actual size={}",
                          i, currentDimension_, vertices[i].size());
            throw std::invalid_argument("Vertex dimension mismatch");
        }
    }
    nCubeVertices_ = vertices;
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set nCubeVertices: size={}", vertices.size());
}

void UniversalEquation::setVertexMomenta(const std::vector<std::vector<long double>>& momenta) {
    for (size_t i = 0; i < momenta.size(); ++i) {
        if (momenta[i].size() != static_cast<size_t>(currentDimension_)) {
            LOG_ERROR_CAT("Simulation", "Momentum dimension mismatch at index {}: expected size={}, actual size={}",
                          i, currentDimension_, momenta[i].size());
            throw std::invalid_argument("Momentum dimension mismatch");
        }
    }
    vertexMomenta_ = momenta;
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set vertexMomenta: size={}", momenta.size());
}

void UniversalEquation::setVertexSpins(const std::vector<long double>& spins) {
    vertexSpins_ = spins;
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set vertexSpins: size={}", spins.size());
}

void UniversalEquation::setVertexWaveAmplitudes(const std::vector<long double>& amplitudes) {
    vertexWaveAmplitudes_ = amplitudes;
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set vertexWaveAmplitudes: size={}", amplitudes.size());
}

void UniversalEquation::setTotalCharge(long double value) {
    totalCharge_ = std::max(value, 1e-30L);
    LOG_DEBUG_CAT(debug_, "Simulation", "Set totalCharge: value={:.6f}", totalCharge_);
}

void UniversalEquation::setMaterialDensity(long double density) {
    materialDensity_ = std::max(density, 1e-30L);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Set materialDensity: value={:.6f}", materialDensity_);
}

void UniversalEquation::evolveTimeStep(long double dt) {
    simulationTime_ += static_cast<float>(dt);
    needsUpdate_ = true;
    LOG_DEBUG_CAT(debug_, "Simulation", "Evolved time step: dt={:.6f}, simulationTime={:.6f}", dt, simulationTime_);
}

void UniversalEquation::updateMomentum() {
    LOG_INFO_CAT("Simulation", "Updating momentum for {} vertices", nCubeVertices_.size());
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
        auto accel = computeGravitationalAcceleration(static_cast<int>(i));
        for (size_t j = 0; j < static_cast<size_t>(currentDimension_); ++j) {
            vertexMomenta_[i][j] += accel[j] * 0.01L * currentDimension_;
            if (std::isnan(vertexMomenta_[i][j]) || std::isinf(vertexMomenta_[i][j])) {
                vertexMomenta_[i][j] = 1e-30L;
            }
        }
    }
    needsUpdate_ = true;
    LOG_INFO_CAT("Simulation", "Momentum updated");
}

void UniversalEquation::advanceCycle() {
    LOG_INFO_CAT("Simulation", "Advancing simulation cycle: simulationTime={:.6f}", simulationTime_);
    updateMomentum();
    evolveTimeStep(0.01L);
    LOG_INFO_CAT("Simulation", "Cycle advanced: simulationTime={:.6f}", simulationTime_);
}

int UniversalEquation::getCurrentDimension() const {
    return currentDimension_;
}

int UniversalEquation::getMode() const {
    return mode_;
}

bool UniversalEquation::getDebug() const {
    return debug_;
}

uint64_t UniversalEquation::getMaxVertices() const {
    return maxVertices_;
}

int UniversalEquation::getMaxDimensions() const {
    return maxDimensions_;
}

long double UniversalEquation::getGodWaveFreq() const {
    return godWaveFreq_;
}

long double UniversalEquation::getInfluence() const {
    return influence_;
}

long double UniversalEquation::getWeak() const {
    return weak_;
}

long double UniversalEquation::getCollapse() const {
    return collapse_;
}

long double UniversalEquation::getTwoD() const {
    return twoD_;
}

long double UniversalEquation::getThreeDInfluence() const {
    return threeDInfluence_;
}

long double UniversalEquation::getOneDPermeation() const {
    return oneDPermeation_;
}

long double UniversalEquation::getNurbMatterStrength() const {
    return nurbMatterStrength_;
}

long double UniversalEquation::getNurbEnergyStrength() const {
    return nurbEnergyStrength_;
}

long double UniversalEquation::getNurbRegularMatterStrength() const {
    return nurbRegularMatterStrength_;
}

long double UniversalEquation::getAlpha() const {
    return alpha_;
}

long double UniversalEquation::getBeta() const {
    return beta_;
}

long double UniversalEquation::getCarrollFactor() const {
    return carrollFactor_;
}

long double UniversalEquation::getMeanFieldApprox() const {
    return meanFieldApprox_;
}

long double UniversalEquation::getAsymCollapse() const {
    return asymCollapse_;
}

long double UniversalEquation::getPerspectiveTrans() const {
    return perspectiveTrans_;
}

long double UniversalEquation::getPerspectiveFocal() const {
    return perspectiveFocal_;
}

long double UniversalEquation::getSpinInteraction() const {
    return spinInteraction_;
}

long double UniversalEquation::getEMFieldStrength() const {
    return emFieldStrength_;
}

long double UniversalEquation::getRenormFactor() const {
    return renormFactor_;
}

long double UniversalEquation::getVacuumEnergy() const {
    return vacuumEnergy_;
}

bool UniversalEquation::getNeedsUpdate() const {
    return needsUpdate_;
}

long double UniversalEquation::getTotalCharge() const {
    return totalCharge_;
}

long double UniversalEquation::getAvgProjScale() const {
    return avgProjScale_;
}

float UniversalEquation::getSimulationTime() const {
    return simulationTime_;
}

long double UniversalEquation::getMaterialDensity() const {
    return materialDensity_;
}

uint64_t UniversalEquation::getCurrentVertices() const {
    return currentVertices_;
}

long double UniversalEquation::getOmega() const {
    return omega_;
}

long double UniversalEquation::getInvMaxDim() const {
    return invMaxDim_;
}

const std::vector<std::vector<long double>>& UniversalEquation::getNCubeVertices() const {
    return nCubeVertices_;
}

const std::vector<std::vector<long double>>& UniversalEquation::getVertexMomenta() const {
    return vertexMomenta_;
}

const std::vector<long double>& UniversalEquation::getVertexSpins() const {
    return vertexSpins_;
}

const std::vector<long double>& UniversalEquation::getVertexWaveAmplitudes() const {
    return vertexWaveAmplitudes_;
}

const std::vector<UE::DimensionInteraction>& UniversalEquation::getInteractions() const {
    return interactions_;
}

const std::vector<long double>& UniversalEquation::getCachedCos() const {
    return cachedCos_;
}

const std::vector<long double>& UniversalEquation::getNurbMatterControlPoints() const {
    return nurbMatterControlPoints_;
}

const std::vector<long double>& UniversalEquation::getNurbEnergyControlPoints() const {
    return nurbEnergyControlPoints_;
}

const std::vector<long double>& UniversalEquation::getNurbRegularMatterControlPoints() const {
    return nurbRegularMatterControlPoints_;
}

const std::vector<long double>& UniversalEquation::getNurbKineticControlPoints() const {
    return nurbKineticControlPoints_;
}

const std::vector<long double>& UniversalEquation::getNurbEMControlPoints() const {
    return nurbEMControlPoints_;
}

const std::vector<long double>& UniversalEquation::getNurbPotentialControlPoints() const {
    return nurbPotentialControlPoints_;
}

const std::vector<long double>& UniversalEquation::getNurbKnots() const {
    return nurbKnots_;
}

const std::vector<long double>& UniversalEquation::getNurbWeights() const {
    return nurbWeights_;
}

const std::vector<UE::DimensionData>& UniversalEquation::getDimensionData() const {
    return dimensionData_;
}

const std::vector<long double>& UniversalEquation::getNCubeVertex(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    return nCubeVertices_[vertexIndex];
}

const std::vector<long double>& UniversalEquation::getVertexMomentum(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    return vertexMomenta_[vertexIndex];
}

long double UniversalEquation::getVertexSpin(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    return vertexSpins_[vertexIndex];
}

long double UniversalEquation::getVertexWaveAmplitude(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    return vertexWaveAmplitudes_[vertexIndex];
}

void UniversalEquation::printVertexTable() const {
    std::stringstream ss;
    ss << "Vertex Table (Dimension: " << currentDimension_ << ", Vertices: " << nCubeVertices_.size() << ")\n";
    ss << "Index | Coordinates | Momentum | Spin | Wave Amplitude\n";
    ss << "------|-------------|----------|------|---------------\n";
    for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
        ss << std::setw(5) << i << " | ";
        for (int j = 0; j < currentDimension_; ++j) {
            printDouble(ss, static_cast<double>(nCubeVertices_[i][j]));
            if (j < currentDimension_ - 1) ss << ", ";
        }
        ss << " | ";
        for (int j = 0; j < currentDimension_; ++j) {
            printDouble(ss, static_cast<double>(vertexMomenta_[i][j]));
            if (j < currentDimension_ - 1) ss << ", ";
        }
        ss << " | ";
        printDouble(ss, static_cast<double>(vertexSpins_[i]));
        ss << " | ";
        printDouble(ss, static_cast<double>(vertexWaveAmplitudes_[i]));
        ss << "\n";
    }
    LOG_INFO_CAT("Simulation", "{}", ss.str());
}

void UniversalEquation::printInteractionTable() const {
    std::stringstream ss;
    ss << "Interaction Table (Vertices: " << interactions_.size() << ")\n";
    ss << "Vertex | Distance | Strength | Vector Potential | God Wave Amp\n";
    ss << "-------|----------|----------|------------------|-------------\n";
    for (size_t i = 0; i < interactions_.size(); ++i) {
        const auto& inter = interactions_[i];
        ss << std::setw(6) << inter.vertexIndex << " | ";
        printDouble(ss, static_cast<double>(inter.distance));
        ss << " | ";
        printDouble(ss, static_cast<double>(inter.strength));
        ss << " | ";
        for (size_t j = 0; j < inter.vectorPotential.size(); ++j) {
            printDouble(ss, static_cast<double>(inter.vectorPotential[j]));
            if (j < inter.vectorPotential.size() - 1) ss << ", ";
        }
        ss << " | ";
        printDouble(ss, static_cast<double>(inter.godWaveAmplitude));
        ss << "\n";
    }
    LOG_INFO_CAT("Simulation", "{}", ss.str());
}

void UniversalEquation::printParameterTable() const {
    std::stringstream ss;
    ss << "Parameter Table\n";
    ss << "Parameter            | Value\n";
    ss << "---------------------|----------\n";
    ss << "Influence            | "; printDouble(ss, static_cast<double>(influence_)) << "\n";
    ss << "Weak                 | "; printDouble(ss, static_cast<double>(weak_)) << "\n";
    ss << "Collapse             | "; printDouble(ss, static_cast<double>(collapse_)) << "\n";
    ss << "TwoD                | "; printDouble(ss, static_cast<double>(twoD_)) << "\n";
    ss << "ThreeDInfluence     | "; printDouble(ss, static_cast<double>(threeDInfluence_)) << "\n";
    ss << "OneDPermeation      | "; printDouble(ss, static_cast<double>(oneDPermeation_)) << "\n";
    ss << "NurbMatterStrength  | "; printDouble(ss, static_cast<double>(nurbMatterStrength_)) << "\n";
    ss << "NurbEnergyStrength  | "; printDouble(ss, static_cast<double>(nurbEnergyStrength_)) << "\n";
    ss << "NurbRegularMatter   | "; printDouble(ss, static_cast<double>(nurbRegularMatterStrength_)) << "\n";
    ss << "Alpha               | "; printDouble(ss, static_cast<double>(alpha_)) << "\n";
    ss << "Beta                | "; printDouble(ss, static_cast<double>(beta_)) << "\n";
    ss << "CarrollFactor       | "; printDouble(ss, static_cast<double>(carrollFactor_)) << "\n";
    ss << "MeanFieldApprox     | "; printDouble(ss, static_cast<double>(meanFieldApprox_)) << "\n";
    ss << "AsymCollapse        | "; printDouble(ss, static_cast<double>(asymCollapse_)) << "\n";
    ss << "PerspectiveTrans    | "; printDouble(ss, static_cast<double>(perspectiveTrans_)) << "\n";
    ss << "PerspectiveFocal    | "; printDouble(ss, static_cast<double>(perspectiveFocal_)) << "\n";
    ss << "SpinInteraction     | "; printDouble(ss, static_cast<double>(spinInteraction_)) << "\n";
    ss << "EMFieldStrength     | "; printDouble(ss, static_cast<double>(emFieldStrength_)) << "\n";
    ss << "RenormFactor        | "; printDouble(ss, static_cast<double>(renormFactor_)) << "\n";
    ss << "VacuumEnergy        | "; printDouble(ss, static_cast<double>(vacuumEnergy_)) << "\n";
    ss << "GodWaveFreq         | "; printDouble(ss, static_cast<double>(godWaveFreq_)) << "\n";
    LOG_INFO_CAT("Simulation", "{}", ss.str());
}

void UniversalEquation::printNURBSTable() const {
    std::stringstream ss;
    ss << "NURBS Table\n";
    ss << "Type               | Control Points | Knots | Weights\n";
    ss << "-------------------|----------------|-------|--------\n";

    auto printVector = [&](const std::vector<long double>& vec) {
        for (size_t i = 0; i < vec.size(); ++i) {
            printDouble(ss, static_cast<double>(vec[i]));
            if (i < vec.size() - 1) ss << ", ";
        }
    };

    ss << "Matter            | ";
    printVector(nurbMatterControlPoints_);
    ss << " | ";
    printVector(nurbKnots_);
    ss << " | ";
    printVector(nurbWeights_);
    ss << "\n";

    ss << "Energy            | ";
    printVector(nurbEnergyControlPoints_);
    ss << " | ";
    printVector(nurbKnots_);
    ss << " | ";
    printVector(nurbWeights_);
    ss << "\n";

    ss << "Regular Matter    | ";
    printVector(nurbRegularMatterControlPoints_);
    ss << " | ";
    printVector(nurbKnots_);
    ss << " | ";
    printVector(nurbWeights_);
    ss << "\n";

    ss << "Kinetic           | ";
    printVector(nurbKineticControlPoints_);
    ss << " | ";
    printVector(nurbKnots_);
    ss << " | ";
    printVector(nurbWeights_);
    ss << "\n";

    ss << "EM Field          | ";
    printVector(nurbEMControlPoints_);
    ss << " | ";
    printVector(nurbKnots_);
    ss << " | ";
    printVector(nurbWeights_);
    ss << "\n";

    ss << "Potential         | ";
    printVector(nurbPotentialControlPoints_);
    ss << " | ";
    printVector(nurbKnots_);
    ss << " | ";
    printVector(nurbWeights_);
    ss << "\n";

    LOG_INFO_CAT("Simulation", "{}", ss.str());
}

UE::DimensionData UniversalEquation::updateCache() {
    LOG_INFO_CAT("Simulation", "Updating cache for dimension {}", currentDimension_);
    UE::EnergyResult energy = compute();
    UE::DimensionData data;
    data.dimension = currentDimension_;
    data.scale = avgProjScale_;
    data.observable = std::max(energy.observable, 1e-30L);
    data.potential = std::max(energy.potential, 1e-30L);
    data.nurbMatter = std::max(energy.nurbMatter, 1e-30L);
    data.nurbEnergy = std::max(energy.nurbEnergy, 1e-30L);
    data.nurbRegularMatter = std::max(energy.nurbRegularMatter, 1e-30L);
    data.spinEnergy = std::max(energy.spinEnergy, 1e-30L);
    data.momentumEnergy = std::max(energy.momentumEnergy, 1e-30L);
    data.fieldEnergy = std::max(energy.fieldEnergy, 1e-30L);
    data.GodWaveEnergy = std::max(energy.GodWaveEnergy, 1e-30L);
    if (static_cast<size_t>(currentDimension_) < dimensionData_.size()) {
        dimensionData_[currentDimension_] = data;
    } else {
        LOG_WARNING_CAT("Simulation", "Dimension {} exceeds dimensionData_ size {}, skipping cache update", currentDimension_, dimensionData_.size());
    }
    LOG_INFO_CAT("Simulation", "Cache updated for dimension {}: {}", currentDimension_, data.toString());
    return data;
}

std::vector<UE::DimensionData> UniversalEquation::computeBatch(int startDim, int endDim) {
    LOG_INFO_CAT("Simulation", "Starting computeBatch: startDim={}, endDim={}", startDim, endDim);
    if (startDim < 1 || endDim > maxDimensions_ || startDim > endDim) {
        LOG_ERROR_CAT("Simulation", "Invalid dimension range: startDim={}, endDim={}, maxDimensions_={}", startDim, endDim, maxDimensions_);
        throw std::invalid_argument("Invalid dimension range");
    }

    std::vector<UE::DimensionData> results;
    results.reserve(endDim - startDim + 1);
    int originalDimension = currentDimension_;
    const long double minValue = 1e-30L; // Minimum non-zero value to prevent underflow

    for (int dim = startDim; dim <= endDim; ++dim) {
        try {
            setCurrentDimension(dim);
            UE::EnergyResult energy = compute();
            UE::DimensionData data;
            data.dimension = dim;
            data.scale = avgProjScale_;
            data.observable = std::max(energy.observable, minValue);
            data.potential = std::max(energy.potential, minValue);
            data.nurbMatter = std::max(energy.nurbMatter, minValue);
            data.nurbEnergy = std::max(energy.nurbEnergy, minValue);
            data.nurbRegularMatter = std::max(energy.nurbRegularMatter, minValue);
            data.spinEnergy = std::max(energy.spinEnergy, minValue);
            data.momentumEnergy = std::max(energy.momentumEnergy, minValue);
            data.fieldEnergy = std::max(energy.fieldEnergy, minValue);
            data.GodWaveEnergy = std::max(energy.GodWaveEnergy, minValue);
            results.push_back(data);
            LOG_INFO_CAT("Simulation", "Computed for dimension {}: observable={:.6f}, nurbMatter={:.6f}, nurbEnergy={:.6f}, nurbRegularMatter={:.6f}",
                         dim, data.observable, data.nurbMatter, data.nurbEnergy, data.nurbRegularMatter);
        } catch (const std::exception& e) {
            LOG_ERROR_CAT("Simulation", "Failed to compute for dimension {}: {}", dim, e.what());
            UE::DimensionData data;
            data.dimension = dim;
            data.scale = avgProjScale_;
            data.observable = minValue;
            data.potential = minValue;
            data.nurbMatter = minValue;
            data.nurbEnergy = minValue;
            data.nurbRegularMatter = minValue;
            data.spinEnergy = minValue;
            data.momentumEnergy = minValue;
            data.fieldEnergy = minValue;
            data.GodWaveEnergy = minValue;
            results.push_back(data);
        }
    }

    setCurrentDimension(originalDimension);
    LOG_INFO_CAT("Simulation", "computeBatch completed: {} results", results.size());
    return results;
}
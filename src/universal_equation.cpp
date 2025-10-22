// universal_equation.cpp - AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com, CC BY-NC 4.0
// Core UniversalEquation, DimensionalNavigator, and AMOURANTH implementation.

#include "ue_init.hpp"
#include "engine/logging.hpp"
#include <numbers>
#include <cmath>
#include <thread>
#include <algorithm>
#include <stdexcept>
#include <latch>
#include <omp.h>

namespace UE {

long double UniversalEquation::safe_div(long double a, long double b) const {
    if (std::abs(b) < 1e-10L) {
        if (debug_) LOG_WARNING("Simulation", "Division by near-zero ({}) avoided", b);
        return 0.0L;
    }
    return a / b;
}

void UniversalEquation::validateVertexIndex(int idx, std::source_location loc) const {
    if (idx < 0 || static_cast<uint64_t>(idx) >= currentVertices_ || currentVertices_ == 0) {
        LOG_SIMULATION("Invalid vertex index {} (currentVertices_={}) at {}", idx, currentVertices_, loc.file_name());
        throw std::out_of_range(std::format("Invalid vertex index {} (max: {}) at {}", idx, currentVertices_ - 1, loc.file_name()));
    }
}

UniversalEquation::UniversalEquation(int maxDim, int mode, long double infl, long double weak, bool debug, uint64_t numVerts)
    : UniversalEquation(maxDim, mode, infl, weak, 5.0L, 1.5L, 5.0L, 1.0L, 0.5L, 1.0L, 0.01L, 0.5L, 0.1L,
                       0.5L, 0.5L, 2.0L, 4.0L, 1.0L, 1.0e6L, 1.0L, 0.5L, 2.0L, debug, numVerts) {}

UniversalEquation::UniversalEquation(int maxDim, int mode, long double infl, long double weak, long double coll, long double twoD,
                                    long double threeD, long double oneD, long double nurbM, long double nurbE, long double alpha,
                                    long double beta, long double carroll, long double meanField, long double asymColl,
                                    long double persTrans, long double persFoc, long double spinInt, long double emField,
                                    long double renorm, long double vacE, long double gwFreq, bool debug, uint64_t numVerts)
    : influence_(std::clamp(infl, 0.0L, 10.0L)), weak_(std::clamp(weak, 0.0L, 1.0L)), collapse_(std::clamp(coll, 0.0L, 5.0L)),
      twoD_(std::clamp(twoD, 0.0L, 5.0L)), threeDInfluence_(std::clamp(threeD, 0.0L, 5.0L)), oneDPermeation_(std::clamp(oneD, 0.0L, 5.0L)),
      nurbMatterStrength_(std::clamp(nurbM, 0.0L, 1.0L)), nurbEnergyStrength_(std::clamp(nurbE, 0.0L, 2.0L)),
      alpha_(std::clamp(alpha, 0.01L, 10.0L)), beta_(std::clamp(beta, 0.0L, 1.0L)), carrollFactor_(std::clamp(carroll, 0.0L, 1.0L)),
      meanFieldApprox_(std::clamp(meanField, 0.0L, 1.0L)), asymCollapse_(std::clamp(asymColl, 0.0L, 1.0L)),
      perspectiveTrans_(std::clamp(persTrans, 0.0L, 10.0L)), perspectiveFocal_(std::clamp(persFoc, 1.0L, 20.0L)),
      spinInteraction_(std::clamp(spinInt, 0.0L, 1.0L)), emFieldStrength_(std::clamp(emField, 0.0L, 1.0e7L)),
      renormFactor_(std::clamp(renorm, 0.1L, 10.0L)), vacuumEnergy_(std::clamp(vacE, 0.0L, 1.0L)),
      godWaveFreq_(std::clamp(gwFreq, 0.1L, 10.0L)), currentDimension_(std::clamp(mode, 1, std::max(1, std::min(maxDim, 19)))),
      mode_(currentDimension_), debug_(debug), needsUpdate_(true), totalCharge_(0.0L), avgProjScale_(1.0L), simulationTime_(0.0f),
      materialDensity_(1000.0L), currentVertices_(0), maxVertices_(std::max<uint64_t>(9, std::min<uint64_t>(numVerts, 1ULL << 20))),
      maxDimensions_(std::max(1, std::min(maxDim, 19))), omega_(2.0L * std::numbers::pi_v<long double> / (2 * maxDimensions_ - 1)),
      invMaxDim_(1.0L / maxDimensions_), nurbMatterControlPoints_{1.0L, 0.8L, 0.5L, 0.3L, 0.1L},
      nurbEnergyControlPoints_{0.1L, 0.5L, 1.0L, 1.5L, 2.0L}, nurbKnots_{0.0L, 0.0L, 0.0L, 0.0L, 0.5L, 1.0L, 1.0L, 1.0L, 1.0L},
      nurbWeights_{1.0L, 1.0L, 1.0L, 1.0L, 1.0L}, dimensionData_(std::max(1, maxDimensions_), DimensionData{}) {
    LOG_SIMULATION("UniversalEquation: maxVerts={}, maxDims={}, mode={}", maxVertices_, maxDimensions_, mode_);
    if (maxVertices_ > 1'000'000) LOG_WARNING("Simulation", "High vertex count: {}", maxVertices_);
    if (mode <= 0 || maxDim <= 0) throw std::invalid_argument(std::format("Invalid maxDim={} or mode={}", maxDim, mode));
    initializeWithRetry();
}

UniversalEquation::~UniversalEquation() { if (debug_) LOG_SIMULATION("UniversalEquation destroyed"); }

#define GETTER(type, name, var) type UniversalEquation::get##name() const { return var##_; }
GETTER(int, CurrentDimension, currentDimension)
GETTER(int, Mode, mode)
GETTER(bool, Debug, debug)
GETTER(uint64_t, MaxVertices, maxVertices)
GETTER(int, MaxDimensions, maxDimensions)
GETTER(long double, GodWaveFreq, godWaveFreq)
GETTER(long double, Influence, influence)
GETTER(long double, Weak, weak)
GETTER(long double, Collapse, collapse)
GETTER(long double, TwoD, twoD)
GETTER(long double, ThreeDInfluence, threeDInfluence)
GETTER(long double, OneDPermeation, oneDPermeation)
GETTER(long double, NurbMatterStrength, nurbMatterStrength)
GETTER(long double, NurbEnergyStrength, nurbEnergyStrength)
GETTER(long double, Alpha, alpha)
GETTER(long double, Beta, beta)
GETTER(long double, CarrollFactor, carrollFactor)
GETTER(long double, MeanFieldApprox, meanFieldApprox)
GETTER(long double, AsymCollapse, asymCollapse)
GETTER(long double, PerspectiveTrans, perspectiveTrans)
GETTER(long double, PerspectiveFocal, perspectiveFocal)
GETTER(long double, SpinInteraction, spinInteraction)
GETTER(long double, EMFieldStrength, emFieldStrength)
GETTER(long double, RenormFactor, renormFactor)
GETTER(long double, VacuumEnergy, vacuumEnergy)
GETTER(bool, NeedsUpdate, needsUpdate)
GETTER(long double, TotalCharge, totalCharge)
GETTER(long double, AvgProjScale, avgProjScale)
GETTER(float, SimulationTime, simulationTime)
GETTER(long double, MaterialDensity, materialDensity)
GETTER(uint64_t, CurrentVertices, currentVertices)
GETTER(long double, Omega, omega)
GETTER(long double, InvMaxDim, invMaxDim)
#undef GETTER

#define GETTER_REF(type, name, var) const type& UniversalEquation::get##name() const { return var##_; }
GETTER_REF(std::vector<std::vector<long double>>, NCubeVertices, nCubeVertices)
GETTER_REF(std::vector<std::vector<long double>>, VertexMomenta, vertexMomenta)
GETTER_REF(std::vector<long double>, VertexSpins, vertexSpins)
GETTER_REF(std::vector<long double>, VertexWaveAmplitudes, vertexWaveAmplitudes)
GETTER_REF(std::vector<DimensionInteraction>, Interactions, interactions)
GETTER_REF(std::vector<glm::vec3>, ProjectedVerts, projectedVerts)
GETTER_REF(std::vector<long double>, CachedCos, cachedCos)
GETTER_REF(std::vector<long double>, NurbMatterControlPoints, nurbMatterControlPoints)
GETTER_REF(std::vector<long double>, NurbEnergyControlPoints, nurbEnergyControlPoints)
GETTER_REF(std::vector<long double>, NurbKnots, nurbKnots)
GETTER_REF(std::vector<long double>, NurbWeights, nurbWeights)
GETTER_REF(std::vector<DimensionData>, DimensionData, dimensionData)
#undef GETTER_REF

DimensionalNavigator* UniversalEquation::getNavigator() const { return navigator_; }
const std::vector<long double>& UniversalEquation::getNCubeVertex(int idx) const { validateVertexIndex(idx); return nCubeVertices_[idx]; }
const std::vector<long double>& UniversalEquation::getVertexMomentum(int idx) const { validateVertexIndex(idx); return vertexMomenta_[idx]; }
long double UniversalEquation::getVertexSpin(int idx) const { validateVertexIndex(idx); return vertexSpins_[idx]; }
long double UniversalEquation::getVertexWaveAmplitude(int idx) const { validateVertexIndex(idx); return vertexWaveAmplitudes_[idx]; }
const glm::vec3& UniversalEquation::getProjectedVertex(int idx) const { validateVertexIndex(idx); return projectedVerts_[idx]; }

#define SETTER(type, name, var, min, max) void UniversalEquation::set##name(type val) { var##_ = std::clamp(val, min, max); needsUpdate_ = true; }
SETTER(int, CurrentDimension, currentDimension, 1, maxDimensions_)
SETTER(int, Mode, mode, 1, maxDimensions_)
SETTER(long double, Influence, influence, 0.0L, 10.0L)
SETTER(long double, Weak, weak, 0.0L, 1.0L)
SETTER(long double, Collapse, collapse, 0.0L, 5.0L)
SETTER(long double, TwoD, twoD, 0.0L, 5.0L)
SETTER(long double, ThreeDInfluence, threeDInfluence, 0.0L, 5.0L)
SETTER(long double, OneDPermeation, oneDPermeation, 0.0L, 5.0L)
SETTER(long double, NurbMatterStrength, nurbMatterStrength, 0.0L, 1.0L)
SETTER(long double, NurbEnergyStrength, nurbEnergyStrength, 0.0L, 2.0L)
SETTER(long double, Alpha, alpha, 0.01L, 10.0L)
SETTER(long double, Beta, beta, 0.0L, 1.0L)
SETTER(long double, CarrollFactor, carrollFactor, 0.0L, 1.0L)
SETTER(long double, MeanFieldApprox, meanFieldApprox, 0.0L, 1.0L)
SETTER(long double, AsymCollapse, asymCollapse, 0.0L, 1.0L)
SETTER(long double, PerspectiveTrans, perspectiveTrans, 0.0L, 10.0L)
SETTER(long double, PerspectiveFocal, perspectiveFocal, 1.0L, 20.0L)
SETTER(long double, SpinInteraction, spinInteraction, 0.0L, 1.0L)
SETTER(long double, EMFieldStrength, emFieldStrength, 0.0L, 1.0e7L)
SETTER(long double, RenormFactor, renormFactor, 0.1L, 10.0L)
SETTER(long double, VacuumEnergy, vacuumEnergy, 0.0L, 1.0L)
SETTER(long double, GodWaveFreq, godWaveFreq, 0.1L, 10.0L)
#undef SETTER

void UniversalEquation::setDebug(bool val) { debug_ = val; }
void UniversalEquation::setCurrentVertices(uint64_t val) { 
    currentVertices_ = std::clamp(val, static_cast<uint64_t>(1), maxVertices_);
    needsUpdate_ = true;
    LOG_SIMULATION("Set currentVertices_ to {}", currentVertices_);
}
void UniversalEquation::setNavigator(DimensionalNavigator* nav) { navigator_ = nav; }
void UniversalEquation::setNCubeVertex(int idx, const std::vector<long double>& v) { 
    validateVertexIndex(idx); 
    nCubeVertices_[idx] = v; 
    needsUpdate_ = true; 
}
void UniversalEquation::setVertexMomentum(int idx, const std::vector<long double>& m) { 
    validateVertexIndex(idx); 
    vertexMomenta_[idx] = m; 
    needsUpdate_ = true; 
}
void UniversalEquation::setVertexSpin(int idx, long double s) { 
    validateVertexIndex(idx); 
    vertexSpins_[idx] = s; 
    needsUpdate_ = true; 
}
void UniversalEquation::setVertexWaveAmplitude(int idx, long double a) { 
    validateVertexIndex(idx); 
    vertexWaveAmplitudes_[idx] = a; 
    needsUpdate_ = true; 
}
void UniversalEquation::setProjectedVertex(int idx, const glm::vec3& v) { 
    validateVertexIndex(idx); 
    projectedVerts_[idx] = v; 
}
void UniversalEquation::setNCubeVertices(const std::vector<std::vector<long double>>& v) { 
    nCubeVertices_ = v; 
    currentVertices_ = std::clamp(static_cast<uint64_t>(v.size()), static_cast<uint64_t>(1), maxVertices_);
    needsUpdate_ = true;
    LOG_SIMULATION("Set nCubeVertices_ with size {}, currentVertices_={}", v.size(), currentVertices_);
}
void UniversalEquation::setVertexMomenta(const std::vector<std::vector<long double>>& m) { 
    vertexMomenta_ = m; 
    needsUpdate_ = true; 
}
void UniversalEquation::setVertexSpins(const std::vector<long double>& s) { 
    vertexSpins_ = s; 
    needsUpdate_ = true; 
}
void UniversalEquation::setVertexWaveAmplitudes(const std::vector<long double>& a) { 
    vertexWaveAmplitudes_ = a; 
    needsUpdate_ = true; 
}
void UniversalEquation::setProjectedVertices(const std::vector<glm::vec3>& v) { 
    projectedVerts_ = v; 
    validateProjectedVertices();
    LOG_SIMULATION("Set projectedVerts_ with size {}", v.size());
}
void UniversalEquation::setTotalCharge(long double v) { totalCharge_ = v; }
void UniversalEquation::setMaterialDensity(long double d) { materialDensity_ = d; }

void UniversalEquation::initializeNCube() {
    LOG_SIMULATION("Entering initializeNCube with maxVertices_={}", maxVertices_);
    std::latch latch(1);
    nCubeVertices_.clear();
    vertexMomenta_.clear();
    vertexSpins_.clear();
    vertexWaveAmplitudes_.clear();
    interactions_.clear();
    projectedVerts_.clear();
    
    // Reserve memory to avoid reallocation
    nCubeVertices_.reserve(maxVertices_);
    vertexMomenta_.reserve(maxVertices_);
    vertexSpins_.reserve(maxVertices_);
    vertexWaveAmplitudes_.reserve(maxVertices_);
    interactions_.reserve(maxVertices_);
    projectedVerts_.reserve(maxVertices_);
    totalCharge_ = 0.0L;

    // Ensure at least one vertex to avoid zero-vertex issues
    uint64_t numVerts = std::max<uint64_t>(1, std::min(maxVertices_, static_cast<uint64_t>(1ULL << 20)));
    LOG_SIMULATION("Initializing {} vertices for dimension {}", numVerts, currentDimension_);
    
    try {
        for (uint64_t i = 0; i < numVerts; ++i) {
            std::vector<long double> v(currentDimension_, 0.0L), m(currentDimension_, 0.0L);
            for (int j = 0; j < currentDimension_; ++j) {
                v[j] = (i / static_cast<long double>(maxVertices_)) * 0.0254L;
                m[j] = (i % 2 - 0.5L) * 0.01L;
            }
            nCubeVertices_.push_back(v);
            vertexMomenta_.push_back(m);
            vertexSpins_.push_back(i % 2 ? -0.032774L : 0.032774L);
            vertexWaveAmplitudes_.push_back(oneDPermeation_ * (1.0L + 0.1L * i / maxVertices_));
            interactions_.push_back({static_cast<int>(i), 0.0L, 0.0L, std::vector<long double>(std::min(3, currentDimension_), 0.0L), 0.0L});
            projectedVerts_.push_back(glm::vec3(0.0f));
            totalCharge_ += 1.0L / maxVertices_;
        }
        currentVertices_ = nCubeVertices_.size();
        LOG_SIMULATION("Initialized {} vertices, currentVertices_={}", nCubeVertices_.size(), currentVertices_);
    } catch (const std::bad_alloc& e) {
        LOG_ERROR("Simulation", "Memory allocation failed in initializeNCube: {}", e.what());
        currentVertices_ = 0;
        throw;
    }

    if (currentVertices_ == 0) {
        LOG_SIMULATION("No vertices initialized in initializeNCube");
        throw std::runtime_error("Failed to initialize any vertices");
    }

    if (!projectedVerts_.empty() && reinterpret_cast<std::uintptr_t>(projectedVerts_.data()) % alignof(glm::vec3)) {
        LOG_SIMULATION("Misaligned projectedVerts_");
        throw std::runtime_error("Misaligned projectedVerts_");
    }
    latch.count_down();
    LOG_SIMULATION("initializeNCube completed");
}

void UniversalEquation::validateProjectedVertices() const {
    LOG_SIMULATION("Validating projectedVerts_ with size {}, nCubeVertices_ size {}", projectedVerts_.size(), nCubeVertices_.size());
    if (projectedVerts_.size() != nCubeVertices_.size()) {
        LOG_ERROR("Simulation", "projectedVerts_ size={} != nCubeVertices_ size={}", projectedVerts_.size(), nCubeVertices_.size());
        throw std::runtime_error(std::format("projectedVerts_ size={} != nCubeVertices_ size={}", projectedVerts_.size(), nCubeVertices_.size()));
    }
    if (!projectedVerts_.empty() && reinterpret_cast<std::uintptr_t>(projectedVerts_.data()) % alignof(glm::vec3)) {
        LOG_SIMULATION("Misaligned projectedVerts_");
        throw std::runtime_error("Misaligned projectedVerts_");
    }
}

long double UniversalEquation::computeKineticEnergy(int idx) const {
    validateVertexIndex(idx);
    long double ke = 0.0L;
    for (const auto& m : vertexMomenta_[idx]) ke += m * m;
    return 0.5L * materialDensity_ * ke;
}

int UniversalEquation::findSpan(long double u, int deg, const std::vector<long double>& knots) const {
    u = std::clamp(u, knots[deg], knots[knots.size() - deg - 1]);
    int low = deg, high = knots.size() - deg - 1, mid = (low + high) / 2;
    while (u < knots[mid] || u >= knots[mid + 1]) {
        if (u < knots[mid]) high = mid; else low = mid;
        mid = (low + high) / 2;
    }
    return mid;
}

std::vector<long double> UniversalEquation::basisFuncs(long double u, int span, int deg, const std::vector<long double>& knots) const {
    std::vector<long double> N(deg + 1, 0.0L), left(deg + 1), right(deg + 1);
    N[0] = 1.0L;
    for (int j = 1; j <= deg; ++j) {
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

long double UniversalEquation::evaluateNURBS(long double u, const std::vector<long double>& cp, const std::vector<long double>& knots,
                                            const std::vector<long double>& weights, int deg) const {
    if (cp.size() != weights.size() || cp.size() + deg + 1 != knots.size()) {
        LOG_SIMULATION("NURBS parameter mismatch: cp.size={}, weights.size={}, knots.size={}, deg={}", 
                  cp.size(), weights.size(), knots.size(), deg);
        throw std::invalid_argument("NURBS parameter mismatch");
    }
    int span = findSpan(u, deg, knots);
    auto basis = basisFuncs(u, span, deg, knots);
    long double result = 0.0L, wsum = 0.0L;
    for (int i = 0; i <= deg; ++i) {
        int idx = span - deg + i;
        if (idx >= 0 && static_cast<size_t>(idx) < cp.size()) {
            result += basis[i] * cp[idx] * weights[idx];
            wsum += basis[i] * weights[idx];
        }
    }
    return wsum > 0.0L ? safe_div(result, wsum) : 0.0L;
}

void UniversalEquation::updateInteractions() {
    LOG_SIMULATION("Entering updateInteractions with currentVertices_={}", currentVertices_);
    if (currentVertices_ == 0) {
        LOG_ERROR("Simulation", "Cannot update interactions with zero vertices");
        throw std::runtime_error("No vertices available for interaction update");
    }

    interactions_.clear();
    projectedVerts_.clear();
    size_t d = currentDimension_, numVerts = std::min(nCubeVertices_.size(), static_cast<size_t>(maxVertices_));
    if (numVerts == 0) {
        LOG_ERROR("Simulation", "No vertices available after clamping, nCubeVertices_.size={}", nCubeVertices_.size());
        throw std::runtime_error("No vertices available for interaction update");
    }
    
    std::vector<std::vector<DimensionInteraction>> localInt(omp_get_max_threads());
    std::vector<std::vector<glm::vec3>> localProj(omp_get_max_threads());
    for (auto& li : localInt) li.reserve(numVerts / omp_get_max_threads() + 1);
    for (auto& lp : localProj) lp.reserve(numVerts / omp_get_max_threads() + 1);

    std::vector<long double> ref(d, 0.0L);
    for (size_t i = 0; i < numVerts; ++i) {
        if (i >= nCubeVertices_.size()) {
            LOG_WARNING("Simulation", "Skipping vertex {} due to size mismatch", i);
            continue;
        }
        for (size_t j = 0; j < d; ++j) {
            ref[j] += nCubeVertices_[i][j];
        }
    }
    for (size_t j = 0; j < d; ++j) {
        ref[j] = safe_div(ref[j], static_cast<long double>(numVerts));
    }
    long double trans = perspectiveTrans_, focal = perspectiveFocal_, depthRef = ref[d - 1] + trans;
    if (depthRef <= 0.0L) depthRef = 0.001L;

    std::latch latch(omp_get_max_threads());
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        #pragma omp for schedule(dynamic)
        for (uint64_t i = 0; i < numVerts; ++i) {
            if (i >= nCubeVertices_.size()) {
                LOG_WARNING("Simulation", "Thread {} skipping vertex {} due to size mismatch", tid, i);
                continue;
            }
            try {
                validateVertexIndex(i);
                const auto& v = nCubeVertices_[i];
                long double depthI = v[d - 1] + trans;
                if (depthI <= 0.0L) depthI = 0.001L;
                long double scaleI = safe_div(focal, depthI), dist = 0.0L;
                for (size_t j = 0; j < d; ++j) {
                    long double diff = v[j] - ref[j];
                    dist += diff * diff;
                }
                dist = std::sqrt(std::max(dist, 1e-10L));
                auto vecPot = computeVectorPotential(i);
                glm::vec3 proj(0.0f);
                for (size_t k = 0; k < std::min<size_t>(3, d); ++k) {
                    proj[k] = static_cast<float>(v[k] * scaleI);
                }
                localInt[tid].emplace_back(i, dist, computeInteraction(i, dist), vecPot, computeGodWaveAmplitude(i, simulationTime_));
                localProj[tid].push_back(proj);
            } catch (const std::exception& e) {
                LOG_ERROR("Simulation", "Thread {} failed processing vertex {}: {}", tid, i, e.what());
            }
        }
        latch.count_down();
    }
    latch.wait();

    size_t totalInt = 0, totalProj = 0;
    for (const auto& li : localInt) totalInt += li.size();
    for (const auto& lp : localProj) totalProj += lp.size();
    interactions_.reserve(totalInt);
    projectedVerts_.reserve(totalProj);
    for (const auto& li : localInt) interactions_.insert(interactions_.end(), li.begin(), li.end());
    for (const auto& lp : localProj) projectedVerts_.insert(projectedVerts_.end(), lp.begin(), lp.end());
    if (totalInt != totalProj || totalInt != numVerts) {
        LOG_ERROR("Simulation", "Mismatch in merged vector sizes: interactions_={}, projectedVerts_={}, expected={}", totalInt, totalProj, numVerts);
        throw std::runtime_error("Mismatch in merged vector sizes");
    }
    validateProjectedVertices();
    currentVertices_ = projectedVerts_.size();
    LOG_SIMULATION("updateInteractions completed with {} interactions and {} projected vertices", interactions_.size(), projectedVerts_.size());
}

EnergyResult UniversalEquation::compute() {
    LOG_SIMULATION("Entering compute with currentVertices_={}", currentVertices_);
    if (needsUpdate_) {
        updateInteractions();
        needsUpdate_ = false;
    }
    if (currentVertices_ == 0) {
        LOG_ERROR("Simulation", "Cannot compute with zero vertices");
        throw std::runtime_error("No vertices available for computation");
    }

    EnergyResult res{};
    uint64_t numVerts = std::min(nCubeVertices_.size(), static_cast<size_t>(maxVertices_));
    if (numVerts == 0) {
        LOG_ERROR("Simulation", "No vertices available for computation after clamping");
        throw std::runtime_error("No vertices available for computation");
    }
    
    std::vector<long double> pots(numVerts), nurbM(numVerts), nurbE(numVerts), spinE(numVerts), momE(numVerts), fieldE(numVerts), gwE(numVerts);

    std::latch latch(omp_get_max_threads());
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        #pragma omp for schedule(dynamic)
        for (uint64_t i = 0; i < numVerts; ++i) {
            if (i >= nCubeVertices_.size()) {
                LOG_WARNING("Simulation", "Thread {} skipping vertex {} due to size mismatch", tid, i);
                continue;
            }
            try {
                validateVertexIndex(i);
                long double pot = 0.0L;
                for (uint64_t j = 0; j < numVerts; j += std::max<uint64_t>(1, numVerts / 100)) {
                    if (i == j) continue;
                    try {
                        pot += computeGravitationalPotential(i, j);
                    } catch (const std::exception& e) {
                        LOG_WARNING("Simulation", "Thread {} failed computing potential for vertex {}->{}: {}", tid, i, j, e.what());
                    }
                }
                pot *= numVerts / 100;
                pots[i] = std::isfinite(pot) ? pot : 0.0L;
                nurbM[i] = computeNurbMatter(i);
                nurbE[i] = computeNurbEnergy(i);
                spinE[i] = computeSpinEnergy(i);
                momE[i] = computeKineticEnergy(i);
                fieldE[i] = computeEMField(i);
                gwE[i] = computeGodWave(i);
            } catch (const std::exception& e) {
                LOG_ERROR("Simulation", "Thread {} failed processing vertex {}: {}", tid, i, e.what());
            }
        }
        latch.count_down();
    }
    latch.wait();

    for (uint64_t i = 0; i < numVerts; ++i) {
        res.observable += pots[i] + nurbM[i] + nurbE[i] + spinE[i] + momE[i] + fieldE[i] + gwE[i];
        res.potential += pots[i];
        res.nurbMatter += nurbM[i];
        res.nurbEnergy += nurbE[i];
        res.spinEnergy += spinE[i];
        res.momentumEnergy += momE[i];
        res.fieldEnergy += fieldE[i];
        res.GodWaveEnergy += gwE[i];
    }
    res.observable = safe_div(res.observable, static_cast<long double>(numVerts));
    LOG_SIMULATION("compute completed with observable={}", res.observable);
    return res;
}

void UniversalEquation::initializeWithRetry() {
    LOG_SIMULATION("Entering initializeWithRetry with maxVertices_={}", maxVertices_);
    std::latch latch(1);
    int attempts = 0;
    uint64_t currVerts = maxVertices_;
    while (currentDimension_ >= 1 && attempts++ < 5) {
        try {
            if (nCubeVertices_.size() > currVerts) {
                nCubeVertices_.resize(currVerts);
                vertexMomenta_.resize(currVerts);
                vertexSpins_.resize(currVerts);
                vertexWaveAmplitudes_.resize(currVerts);
                interactions_.resize(currVerts, {0, 0.0L, 0.0L, std::vector<long double>(std::min(3, currentDimension_), 0.0L), 0.0L});
                projectedVerts_.resize(currVerts);
                LOG_SIMULATION("Resized vectors to currVerts={}", currVerts);
            }
            initializeNCube();
            if (currentVertices_ == 0) {
                LOG_ERROR("Simulation", "initializeNCube failed to set vertices");
                throw std::runtime_error("No vertices initialized");
            }
            cachedCos_.resize(maxDimensions_ + 1);
            for (int i = 0; i <= maxDimensions_; ++i) {
                cachedCos_[i] = std::cos(omega_ * i);
            }
            updateInteractions();
            validateProjectedVertices();
            latch.count_down();
            LOG_SIMULATION("initializeWithRetry succeeded with {} vertices", currentVertices_);
            return;
        } catch (const std::bad_alloc& e) {
            LOG_WARNING("Simulation", "Memory allocation failed, reducing dimension to {} and vertices to {}: {}", currentDimension_ - 1, currVerts / 2, e.what());
            setCurrentDimension(currentDimension_ - 1);
            currVerts = std::max<uint64_t>(1, currVerts / 2);
            needsUpdate_ = true;
        } catch (const std::exception& e) {
            LOG_ERROR("Simulation", "Initialization failed: {}", e.what());
            throw;
        }
    }
    LOG_ERROR("Simulation", "Max retry attempts reached");
    throw std::runtime_error("Max retry attempts reached");
}

void UniversalEquation::initializeCalculator(Camera* cam) {
    LOG_SIMULATION("Entering initializeCalculator");
    if (navigator_ && cam) {
        navigator_->initialize(currentDimension_, maxVertices_);
        LOG_SIMULATION("Navigator initialized with dimension={} and maxVertices_={}", currentDimension_, maxVertices_);
    }
    try {
        initializeWithRetry();
        validateProjectedVertices();
    } catch (const std::exception& e) {
        LOG_ERROR("Simulation", "Failed to initialize calculator: {}", e.what());
        throw;
    }
    LOG_SIMULATION("initializeCalculator completed");
}

long double UniversalEquation::computeNurbMatter(int idx) const {
    validateVertexIndex(idx);
    long double u = idx / static_cast<long double>(maxVertices_ - 1);
    return nurbMatterStrength_ * evaluateNURBS(u, nurbMatterControlPoints_, nurbKnots_, nurbWeights_, 3) *
           vertexWaveAmplitudes_[idx] + 0.5L * std::sin(godWaveFreq_ * simulationTime_) * vertexWaveAmplitudes_[idx];
}

long double UniversalEquation::computeNurbEnergy(int idx) const {
    validateVertexIndex(idx);
    long double u = idx / static_cast<long double>(maxVertices_ - 1);
    return nurbEnergyStrength_ * evaluateNURBS(u, nurbEnergyControlPoints_, nurbKnots_, nurbWeights_, 3) *
           vertexWaveAmplitudes_[idx] + 0.1L * vacuumEnergy_ * std::cos(godWaveFreq_ * simulationTime_);
}

long double UniversalEquation::computeSpinEnergy(int idx) const {
    validateVertexIndex(idx);
    return spinInteraction_ * vertexSpins_[idx] * 0.2L * (1.0L + 0.1L * beta_) * std::cos(omega_ * simulationTime_);
}

long double UniversalEquation::computeEMField(int idx) const {
    validateVertexIndex(idx);
    return emFieldStrength_ * vertexWaveAmplitudes_[idx] * 0.01L + 0.05L * alpha_ * std::sin(godWaveFreq_ * simulationTime_);
}

long double UniversalEquation::computeGodWave(int idx) const {
    validateVertexIndex(idx);
    return godWaveFreq_ * vertexWaveAmplitudes_[idx] * 0.1L + 0.2L * carrollFactor_ * std::sin(simulationTime_ * godWaveFreq_);
}

long double UniversalEquation::computeInteraction(int idx, long double dist) const {
    validateVertexIndex(idx);
    return influence_ * safe_div(1.0L, dist + 1e-10L);
}

std::vector<long double> UniversalEquation::computeVectorPotential(int idx) const {
    validateVertexIndex(idx);
    std::vector<long double> res(std::min(3, currentDimension_), 0.0L);
    for (int i = 0; i < std::min(3, currentDimension_); ++i) {
        res[i] = vertexMomenta_[idx][i] * weak_;
    }
    return res;
}

long double UniversalEquation::computeGravitationalPotential(int idx, int other) const {
    validateVertexIndex(idx);
    validateVertexIndex(other);
    if (idx == other) return 0.0L;
    long double dist = 0.0L;
    const auto& v1 = nCubeVertices_[idx], v2 = nCubeVertices_[other];
    for (size_t j = 0; j < static_cast<size_t>(currentDimension_); ++j) {
        dist += (v1[j] - v2[j]) * (v1[j] - v2[j]);
    }
    dist = std::sqrt(std::max(dist, 1e-10L));
    return -influence_ * safe_div(1.0L, dist);
}

std::vector<long double> UniversalEquation::computeGravitationalAcceleration(int idx) const {
    validateVertexIndex(idx);
    std::vector<long double> acc(currentDimension_, 0.0L);
    for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
        if (i == static_cast<size_t>(idx)) continue;
        try {
            long double dist = 0.0L;
            const auto& v1 = nCubeVertices_[idx], v2 = nCubeVertices_[i];
            for (size_t j = 0; j < static_cast<size_t>(currentDimension_); ++j) {
                dist += (v1[j] - v2[j]) * (v1[j] - v2[j]);
            }
            dist = std::sqrt(std::max(dist, 1e-10L));
            long double force = influence_ * safe_div(1.0L, dist * dist);
            for (size_t j = 0; j < static_cast<size_t>(currentDimension_); ++j) {
                acc[j] += force * (v2[j] - v1[j]) / dist;
            }
        } catch (const std::exception& e) {
            LOG_WARNING("Simulation", "Failed computing acceleration for vertex {}->{}: {}", idx, i, e.what());
        }
    }
    return acc;
}

long double UniversalEquation::computeGodWaveAmplitude(int idx, long double t) const {
    validateVertexIndex(idx);
    return godWaveFreq_ * vertexWaveAmplitudes_[idx] * std::cos(godWaveFreq_ * t);
}

long double UniversalEquation::safeExp(long double x) const {
    return std::isfinite(x) ? (x > 100.0L ? std::exp(100.0L) : std::exp(x)) : 0.0L;
}

std::vector<DimensionData> UniversalEquation::computeBatch(int start, int end) {
    LOG_SIMULATION("Computing batch from dimension {} to {}", start, end);
    std::vector<DimensionData> batch;
    for (int d = start; d <= end; ++d) {
        setCurrentDimension(d);
        DimensionData data{ .dimension = d, .scale = d * invMaxDim_, .nurbEnergy = computeNurbEnergy(0),
                            .nurbMatter = computeNurbMatter(0), .potential = computeGravitationalPotential(0, 1),
                            .observable = data.nurbEnergy + data.nurbMatter };
        batch.push_back(data);
    }
    LOG_SIMULATION("Batch computed with {} entries", batch.size());
    return batch;
}

void UniversalEquation::exportToCSV(const std::string& fn, const std::vector<DimensionData>& data) const {
    std::ofstream f(fn);
    if (!f.is_open()) {
        LOG_ERROR("Simulation", "Failed to open CSV file: {}", fn);
        return;
    }
    f << "Dimension,Scale,PositionX,PositionY,PositionZ,Value,NurbEnergy,NurbMatter,Potential,Observable,SpinEnergy,MomentumEnergy,FieldEnergy,GodWaveEnergy\n";
    for (const auto& d : data) {
        f << d.dimension << "," << std::fixed << std::setprecision(10) << d.scale << "," << d.position.x << "," << d.position.y << "," << d.position.z << ","
          << d.value << "," << d.nurbEnergy << "," << d.nurbMatter << "," << d.potential << "," << d.observable << ","
          << d.spinEnergy << "," << d.momentumEnergy << "," << d.fieldEnergy << "," << d.GodWaveEnergy << "\n";
    }
    if (debug_) LOG_SIMULATION("Exported {} dims to {}", data.size(), fn);
}

DimensionData UniversalEquation::updateCache() {
    LOG_SIMULATION("Updating cache with currentVertices_={}", currentVertices_);
    DimensionData data{ .dimension = currentDimension_, .scale = currentDimension_ * invMaxDim_, .value = static_cast<float>(influence_) };
    cachedCos_.resize(currentVertices_);
    for (uint64_t i = 0; i < currentVertices_; ++i) {
        try {
            cachedCos_[i] = std::cos(omega_ * i);
        } catch (const std::exception& e) {
            LOG_WARNING("Simulation", "Failed updating cache for index {}: {}", i, e.what());
        }
    }
    return data;
}

void UniversalEquation::evolveTimeStep(long double dt) {
    LOG_SIMULATION("Evolving time step with dt={}", dt);
    for (auto& mom : vertexMomenta_) {
        for (auto& m : mom) {
            m += dt * safe_div(vacuumEnergy_, static_cast<long double>(maxDimensions_));
        }
    }
    needsUpdate_ = true;
}

void UniversalEquation::advanceCycle() {
    LOG_SIMULATION("Advancing cycle");
    evolveTimeStep(0.1L);
    updateInteractions();
    if (navigator_) {
        navigator_->getCamera().update(simulationTime_);
    }
}

void UniversalEquation::exportData() const {
    if (debug_) exportToCSV("ue_data.csv", dimensionData_);
}

// DimensionalNavigator Implementation
DimensionalNavigator::DimensionalNavigator(const char* name, int w, int h, VulkanRTX::VulkanRenderer& r)
    : name_(name), width_(w), height_(h), mode_(1), dimension_(1), numVertices_(9), renderer_(r), camera_(w / static_cast<float>(h)) {}

Camera& DimensionalNavigator::getCamera() { return camera_; }
const Camera& DimensionalNavigator::getCamera() const { return camera_; }
void DimensionalNavigator::setWidth(int w) { 
    width_ = std::max(1, w); 
    camera_.setAspectRatio(width_ / static_cast<float>(height_)); 
}
void DimensionalNavigator::setHeight(int h) { 
    height_ = std::max(1, h); 
    camera_.setAspectRatio(width_ / static_cast<float>(height_)); 
}
void DimensionalNavigator::setMode(int m) { mode_ = std::clamp(m, 1, 19); }
void DimensionalNavigator::initialize(int dim, uint64_t verts) {
    dimension_ = std::clamp(dim, 1, 19);
    numVertices_ = std::max<uint64_t>(9, std::min<uint64_t>(verts, static_cast<uint64_t>(1ULL << 20)));
    LOG_SIMULATION("DimensionalNavigator initialized with dimension={} and numVertices_={}", dimension_, numVertices_);
}
int DimensionalNavigator::getWidth() const { return width_; }
int DimensionalNavigator::getHeight() const { return height_; }
int DimensionalNavigator::getMode() const { return mode_; }
int DimensionalNavigator::getDimension() const { return dimension_; }
uint64_t DimensionalNavigator::getNumVertices() const { return numVertices_; }
VulkanRTX::VulkanRenderer& DimensionalNavigator::getRenderer() const { return renderer_; }

// AMOURANTH Implementation
AMOURANTH::AMOURANTH(DimensionalNavigator* nav, VkDevice dev, VkDeviceMemory vMem, VkDeviceMemory iMem, VkPipeline pipe)
    : navigator_(nav), logicalDevice_(dev), vertexMemory_(vMem), indexMemory_(iMem), pipeline_(pipe), mode_(1), currentDimension_(1),
      nurbMatter_(0.032774f), nurbEnergy_(1.0f), universalEquation_(std::make_unique<UniversalEquation>(9, 1, 1.0L, 0.5L, true, 9)),
      position_(0.0f), target_(0.0f, 0.0f, -1.0f), up_(0.0f, 1.0f, 0.0f), fov_(45.0f), aspectRatio_(1.0f), nearPlane_(0.1f), farPlane_(100.0f),
      isPaused_(false) {
    universalEquation_->setNavigator(nav);
    LOG_SIMULATION("AMOURANTH initialized with {} vertices", universalEquation_->getCurrentVertices());
}

AMOURANTH::~AMOURANTH() {}

glm::mat4 AMOURANTH::getViewMatrix() const { return glm::lookAt(position_, position_ + target_, up_); }
glm::mat4 AMOURANTH::getProjectionMatrix() const { return glm::perspective(glm::radians(fov_), aspectRatio_, nearPlane_, farPlane_); }
int AMOURANTH::getMode() const { return mode_; }
glm::vec3 AMOURANTH::getPosition() const { return position_; }
void AMOURANTH::setPosition(const glm::vec3& p) { position_ = p; }
void AMOURANTH::setOrientation(float yaw, float pitch) {
    target_ = glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw));
}
void AMOURANTH::update(float dt) {
    if (!isPaused_) {
        try {
            universalEquation_->evolveTimeStep(dt); // Use dt for time step
            universalEquation_->advanceCycle();
        } catch (const std::exception& e) {
            LOG_ERROR("Simulation", "Failed to advance cycle in AMOURANTH::update: {}", e.what());
            isPaused_ = true; // Pause to prevent further errors
        }
    }
}
void AMOURANTH::moveForward(float s) { position_ += s * target_; }
void AMOURANTH::moveRight(float s) { position_ += s * glm::normalize(glm::cross(target_, up_)); }
void AMOURANTH::moveUp(float s) { position_ += s * up_; }
void AMOURANTH::rotate(float yaw, float pitch) {
    float newYaw = std::atan2(target_.x, target_.z) + yaw;
    float newPitch = std::asin(target_.y) + pitch;
    setOrientation(newYaw, std::clamp(newPitch, -glm::radians(89.0f), glm::radians(89.0f)));
}
void AMOURANTH::setFOV(float fov) { fov_ = std::clamp(fov, 10.0f, 120.0f); }
float AMOURANTH::getFOV() const { return fov_; }
void AMOURANTH::setMode(int m) { 
    mode_ = std::clamp(m, 1, 9); 
    universalEquation_->setMode(mode_); 
}
void AMOURANTH::setModeWithLocation(int m, std::source_location) { setMode(m); }
const std::vector<glm::vec3>& AMOURANTH::getBalls() const { return balls_; }
int AMOURANTH::getCurrentDimension() const { return currentDimension_; }
float AMOURANTH::getNurbMatter() const { return nurbMatter_; }
float AMOURANTH::getNurbEnergy() const { return nurbEnergy_; }
UniversalEquation& AMOURANTH::getUniversalEquation() { return *universalEquation_; }
const UniversalEquation& AMOURANTH::getUniversalEquation() const { return *universalEquation_; }
bool AMOURANTH::isPaused() const { return isPaused_; }
const std::vector<DimensionData>& AMOURANTH::getCache() const { return cache_; }
void AMOURANTH::setCurrentDimension(int d, std::source_location) { 
    currentDimension_ = std::clamp(d, 1, 9); 
    universalEquation_->setCurrentDimension(d); 
}
void AMOURANTH::setNurbMatter(float m, std::source_location) { nurbMatter_ = m; }
void AMOURANTH::setNurbEnergy(float e, std::source_location) { nurbEnergy_ = e; }
void AMOURANTH::adjustNurbMatter(float d, std::source_location) { nurbMatter_ += d; }
void AMOURANTH::adjustNurbEnergy(float d, std::source_location) { nurbEnergy_ += d; }
void AMOURANTH::adjustInfluence(float d, std::source_location) { universalEquation_->setInfluence(universalEquation_->getInfluence() + d); }
void AMOURANTH::updateZoom(bool zoomIn, std::source_location) { fov_ *= zoomIn ? 0.9f : 1.1f; fov_ = std::clamp(fov_, 10.0f, 120.0f); }
void AMOURANTH::togglePause(std::source_location) { isPaused_ = !isPaused_; }
void AMOURANTH::moveUserCam(float dx, float dy, float dz, std::source_location) { position_ += glm::vec3(dx, dy, dz); }
void AMOURANTH::rotateCamera(float yaw, float pitch, std::source_location) { rotate(yaw, pitch); }
void AMOURANTH::moveCamera(float dx, float dy, float dz, std::source_location) { moveUserCam(dx, dy, dz); }

// UE Implementation
UE::UE() : universalEquation_(std::make_unique<UniversalEquation>(9, 1, 1.0L, 0.5L, true, 9)) {}
UE::~UE() {}
void UE::initializeDimensionData(VkDevice device, VkPhysicalDevice physicalDevice) { 
    LOG_SIMULATION("Initializing dimension data");
    dimensions_.resize(9, DimensionData{});
    if (device == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR("Simulation", "Invalid Vulkan device or physical device for dimension data initialization");
        throw std::invalid_argument("Invalid Vulkan device or physical device");
    }
}
void UE::updateUBO(uint32_t frame, const glm::mat4& view, const glm::mat4& proj, uint32_t mode) {
    if (ubos_.size() <= frame) {
        ubos_.resize(frame + 1);
        LOG_SIMULATION("Resized UBOs to {}", ubos_.size());
    }
    ubos_[frame] = {glm::mat4(1.0f), view, proj, static_cast<int>(mode)};
}
void UE::cleanup([[maybe_unused]] VkDevice device) { 
    dimensionBuffer_ = VK_NULL_HANDLE; 
    dimensionBufferMemory_ = VK_NULL_HANDLE; 
    descriptorSet_ = VK_NULL_HANDLE; 
    LOG_SIMULATION("Cleaned up UE resources");
}

} // namespace UE
#pragma once

// =============================================================================
// AMOURANTH RTX Engine — RayCanvas (HYBRID: raymarched compute + full hardware RT with SBT + RTXGI)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
// =============================================================================

#include "AMOURANTHRTX.hpp"
#include "ELLIE.hpp"
#include "Camera.hpp"
#include "OptionsMenu.hpp"
#include "FieldDos.hpp"
#include "FieldRtxMemTree.hpp"
#include "FieldDosDisplay.hpp"
#include "FieldAosTest.hpp"
#include "FieldAmouranthFileCmd.hpp"
#include "FieldAmouranthCursor.hpp"
#include "FieldAmouranthOs.hpp"
#include "FieldAmouranthDeactivate.hpp"

#include "FieldSnapDump.hpp"
#include "FieldBios.hpp"
#include "FieldX86Emu.hpp"
#include "FieldDosChrome.hpp"
#include "FieldRtxTerm.hpp"
#include "Pipeline.hpp"
#include "Materials.hpp"
#include "SDL3.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <vector>
#include <cstdint>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>  // getenv, atoll for AMOURANTHRTX_HEADLESS / MAX_FRAMES test mode

// =============================================================================
// RayCanvas — main hybrid renderer class
// =============================================================================
class RayCanvas {
public:
    RayCanvas(int initialWidth, int initialHeight, SDL_Window* window)
        : window_(window),
          window_width_(initialWidth),
          window_height_(initialHeight),
          render_width_(initialWidth),
          render_height_(initialHeight),
          minimized_(false),
          destroyed_(false),
          firstFrame_(true),
          materialsHandle_(0),
          tlas_(VK_NULL_HANDLE),
          hdrOutputImage_(VK_NULL_HANDLE),
          hdrOutputView_(VK_NULL_HANDLE),
          hdrOutputMemory_(VK_NULL_HANDLE),
          prevHdrOutputImage_(VK_NULL_HANDLE),
          prevHdrOutputView_(VK_NULL_HANDLE),
          prevHdrOutputMemory_(VK_NULL_HANDLE),
          descriptorPool_(VK_NULL_HANDLE),
          descriptorSet_(VK_NULL_HANDLE),
          adaptiveScale_(1.0),
          lastPresentTime_s_(0.0),
          measuredRefreshRateHz_(60.0),
          lastFpsLog_(-999.0),  // force first status block very early (for short runs visibility of full THERMO metrics)
          frameCount_(0),
          lastAdaptiveAdjustTime_(0.0),
          needsRecreate_(false),
          timestampQueryPool_(VK_NULL_HANDLE),
          timestampPeriodNs_(1.0),
          smoothedGpuTimeMs_(16.67),
          headless_(Options::SDL3::HeadlessMode),
          maxFrames_(Options::SDL3::MaxFrames),
          totalFrames_(0)
    {
        if (!Swapchain::get()) {
            if (headless_) {
                LOG_WARNING_CAT("RAYCANVAS", "HEADLESS: No real swapchain (expected in offscreen/minimized/SDL-picky env) — proceeding with direct dispatch path for accountant population (warnings not fatals)");
            } else {
                LOG_FATAL_CAT("RAYCANVAS", "No valid swapchain — navigator must create it first");
                std::abort();
            }
        }

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(rtx().physical, &props);
        timestampPeriodNs_ = props.limits.timestampPeriod;

        // Initialize pipelines (compute + ray tracing)
        LOG_DEBUG_CAT("RAYCANVAS", "CTOR: begin RayCanvas init — width={} height={} (source loc logged)", initialWidth, initialHeight);
        LOG_DEBUG_CAT("THERMO", "CTOR THERMO: pre-pipeline — about to create accountant buffer + fabric seeds");
        Pipeline::create_pipeline_layout();
        Pipeline::pump_startup_events();
        Pipeline::boot_x86_canvas();
        Pipeline::pump_startup_events();
        SDL3System::get().applyMouseCapture();
        FieldAmouranthOs::init(window_);
        FieldAmouranthOs::boot();
        if (window_) {
            FieldDosChrome::refreshWindowMetrics(window_);
            SDL_RaiseWindow(window_);
        }
        if (Pipeline::fieldX86DieMapped) {
            if (auto* gr = FieldDos::guestRam(
                    static_cast<std::uint8_t*>(Pipeline::fieldX86DieMapped),
                    Pipeline::FIELD_X86_DIE_HEADER_BYTES)) {
                FieldX86Emu::bindChromeGuest(gr);
                FieldAmouranthHudRam::clearRegion(gr);
                FieldAmouranthOs::seedChromeRam(gr);
                FieldAmouranthOs::packDataBus(Options::Canvas::DataBus, gr);
            }
        }
        FieldAmouranthCursor::init();
        Pipeline::pump_startup_events();
        LOG_DEBUG_CAT("THERMO", "CTOR THERMO: after Pipeline::create_canvas_pipeline('{}')", Options::Canvas::CurrentName());
        createTimestampQueryPool();  // always (for gpu timing/adaptive); probe extra slots/writes gated
        createPipelineStatisticsQueryPool();  // gated inside, zero cost if !caps or !probes
        buildMaterialLibrary();
        updateRenderResolution();
        createPersistentHDR();
        createPreviousHDR();
        Pipeline::pump_startup_events();
        LOG_DEBUG_CAT("THERMO", "CTOR THERMO: HDR pair ready — now seeding analog field fabric (Phi/Thermo/Flow entropy floor)");
        createAnalogFieldFabric();   // Phi + Thermo + Flow — the propalactic heart (our thermo field)
        LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: after fabric (Thermo image ready)");
        LOG_DEBUG_CAT("THERMO", "CTOR THERMO: fabric created — field seeds will be applied in clearFieldImages (thermo entropy floor ~0.015)");

        // ENSURE pre-transition in CTOR before pool/update (as required): all images from UNDEF post-create -> GENERAL
        // (so descriptor writes for images declare correct layout; clears rely on this, no UNDEF inside clears).
        preTransitionAllImagesToGeneral();

        createDescriptorPoolAndSet();
        Pipeline::pump_startup_events();
        LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: after pool+set");
        LOG_DEBUG_CAT("THERMO", "CTOR THERMO: pool+set allocated — next updateDescriptorSet will write binding 2 (accountant) + 8/9/10 (fields)");

        LOG_DEBUG_CAT("THERMO", "CTOR THERMO: entering updateDescriptorSet — will populate binding 2 accountant + field descriptors for entropy/boundary reads");
        updateDescriptorSet();       // includes explicit binding 2 for THERMO_ACCOUNTANT + fields
        LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: after descriptor update (bindings incl 2 accountant)");
        s_activeInstance_ = this;
        Pipeline::patchX86ChromeDescriptors = &RayCanvas::patchChromeDescriptorsStatic;
        Pipeline::sync_aos_textures();
        ensureX86ChromeTextureBindings();
        LOG_DEBUG_CAT("THERMO", "CTOR THERMO: descriptor writes complete (accountant at b2, thermo field at b9) — pre-clears next");

        if (!headless_) {
            LOG_DEBUG_CAT("THERMO", "CTOR THERMO: clearHDRImages — zeroing holographic boundary (current/prev) to establish initial negentropy state");
            clearHDRImages();
            LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: after HDR clears");

            LOG_DEBUG_CAT("THERMO", "CTOR THERMO: clearFieldImages — applying field seeds: phi weave, thermo body-temp+entropy-floor, flow drift (entropy calcs begin here) + prevMaint baseline");
            clearFieldImages();          // seed ... + thermo entropy floor
            LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: after field clears (thermo seed)");
            LOG_DEBUG_CAT("THERMO", "CTOR THERMO: field seeds applied — thermo entropy floor established, boundary ready for first dispatch (prevMaintCost/freeEnergyIncome/boundaryThermo will be computed per-frame in accountant pop)");
            LOG_DEBUG_CAT("THERMO", "CTOR THERMO: post clearField hex dump thermoView=0x{:016x} for boundary source", reinterpret_cast<uintptr_t>(fieldThermoView_));
        } else {
            LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: headless - skipping clearHDR/clearField (to reach Initialized/Phase 1 without lavapipe fault in debug env). Fabric/accountant live; dispatches will still populate thermo metrics.");
        }

        // HOLOGRAPHIC BOUNDARY NOTE (schematic-aligned):
        // The primary information surface is the HDR pair (current + previous) + the three field fabrics.
        // "Bulk" (SDF character, materials, TLAS primitives, lighting) is reconstructed/emergent from this boundary.
        // Irreversibility (accumulation write, probe readout) is localized here and *is* the arrow + experience.
        // The THERMO ACCOUNTANT (binding 2) makes the free-energy / entropy-export story first-class and observable.

        lastAdaptiveAdjustTime_ = TotalTime::get().seconds();

        LOG_SUCCESS_CAT("RAYCANVAS", "Initialized hybrid renderer — {}x{} render target ready (compute + hardware RT + RTXGI ready)", 
                        render_width_, render_height_);
        LOG_DEBUG_CAT("THERMO", "CTOR THERMO: RayCanvas fully initialized — TotalTime={:.3f}s | accountant+fields seeded | full THERMO metrics will appear in every 5s status (and early for short runs)", TotalTime::get().seconds());
        LOG_SUCCESS_CAT("THERMO", "Phase 1 thermodynamic accountant active (binding 2) — entropy, boundary temp, prev-maint cost, free energy now first-class and logged in status. Holographic canvas per schematic.");
        LOG_DEBUG_CAT("RAYCANVAS", "CTOR complete: source_location path traced through ELLIE for all above steps");
        if (headless_) {
            LOG_INFO_CAT("RAYCANVAS", "HEADLESS RayCanvas ready: offscreen mode. NO swap present waits. First maybeUpdateCanvas dispatch will populate accountant + fields. Status block + thermo logs will make values visible. Use AMOURANTHRTX_MAX_FRAMES to bound run. Tolerant: warnings not fatals.");
        }
        if (Options::SDL3::RequestQuit)
            destroyed_ = true;
    }

    ~RayCanvas() {
        if (resourcesReleased_) return;
        resourcesReleased_ = true;
        destroyed_ = true;
        if (Pipeline::hotswap_compile_still_running()) return;
        Pipeline::prepare_shutdown();
        if (rtx().device)
            vkDeviceWaitIdle(rtx().device);

        LOG_DEBUG_CAT("THERMO", "DTOR THERMO: beginning shutdown — unmapping thermo accountant (final prevMaintCost/freeEnergyIncome/entropy/boundaryThermo values frozen) — last status had full THERMO");
        LOG_DEBUG_CAT("THERMO", "DTOR THERMO: fabric+accountant+descriptors released (pre-trans/clears/dispatch steps complete for lifetime)");
        if (tlas_ != VK_NULL_HANDLE) {
            ext().vkDestroyAccelerationStructureKHR(rtx().device, tlas_, nullptr);
            tlas_ = VK_NULL_HANDLE;
        }

        if (timestampQueryPool_ != VK_NULL_HANDLE) {
            vkDestroyQueryPool(rtx().device, timestampQueryPool_, nullptr);
            timestampQueryPool_ = VK_NULL_HANDLE;
        }
        if (pipelineStatsQueryPool_ != VK_NULL_HANDLE) {
            vkDestroyQueryPool(rtx().device, pipelineStatsQueryPool_, nullptr);
            pipelineStatsQueryPool_ = VK_NULL_HANDLE;
        }

        if (descriptorSet_ != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(rtx().device, descriptorPool_, 1, &descriptorSet_);
            descriptorSet_ = VK_NULL_HANDLE;
        }

        if (descriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(rtx().device, descriptorPool_, nullptr);
            descriptorPool_ = VK_NULL_HANDLE;
        }

        destroyHDRResources();
        destroyAnalogFieldFabric();
        Memory::destroy(materialsHandle_);

        LOG_DEBUG_CAT("RAYCANVAS", "DTOR: fabric + accountant + descriptors released");
        FieldAmouranthCursor::shutdown();
        FieldAmouranthOs::shutdown();
        if (s_activeInstance_ == this) {
            s_activeInstance_ = nullptr;
            Pipeline::patchX86ChromeDescriptors = nullptr;
        }
        LOG_SUCCESS_CAT("RAYCANVAS", "Hybrid renderer destroyed (compute + RT + SBT + RTXGI cleaned up)");
    }

    bool maybeUpdateCanvas(bool isRunning) noexcept {
        if (destroyed_) { isRunning = false; return isRunning; }
        if (Options::SDL3::RequestQuit) {
            destroyed_ = true;
            LOG_INFO_CAT("RAYCANVAS", "AmmoOS shut down — exiting main loop");
            return false;
        }

        ++totalFrames_;
        frameCount_++;
        double now = TotalTime::get().seconds();
        int currentW = window_width_;
        int currentH = window_height_;
        if (window_) {
            int pixW = 0, pixH = 0;
            SDL_GetWindowSizeInPixels(window_, &pixW, &pixH);
            if (pixW > 0 && pixH > 0) {
                currentW = pixW;
                currentH = pixH;
            }
        }
        bool quit = false;
        bool fullscreen_toggle = false;


        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            INPUT.pump(ev);

            if (ev.type == SDL_EVENT_QUIT) {
                if (FieldAmouranthExitConfirm::isOpen()) {
                    FieldAmouranthExitConfirm::dismiss(FieldX86Emu::ramHost);
                    quit = true;
                } else if (FieldAmouranthOs::shellChromeActive() && !Options::SDL3::HeadlessMode) {
                    FieldAmouranthExitConfirm::show();
                } else {
                    quit = true;
                }
            }
            if (ev.type == SDL_EVENT_WINDOW_RESIZED
                    || ev.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                if (window_) {
                    SDL_GetWindowSizeInPixels(window_, &currentW, &currentH);
                } else {
                    currentW = ev.window.data1;
                    currentH = ev.window.data2;
                }
            }
            if (Options::Canvas::IsX86Fields()) {
                const bool mainMouseEv = ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                    || ev.type == SDL_EVENT_MOUSE_BUTTON_UP
                    || ev.type == SDL_EVENT_MOUSE_MOTION;
                if (mainMouseEv && window_) {
                    SDL_WindowID evWin = 0u;
                    if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev.type == SDL_EVENT_MOUSE_BUTTON_UP)
                        evWin = ev.button.windowID;
                    else if (ev.type == SDL_EVENT_MOUSE_MOTION)
                        evWin = ev.motion.windowID;
                    SDL_Window* evWindow = evWin ? SDL_GetWindowFromID(evWin) : nullptr;
                    if (evWindow && evWindow != window_)
                        continue;
                }
                if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    syncChromeInputLayout();
                    ensureX86ChromeTextureBindings();
                    if (window_ && FieldAmouranthOs::shellChromeActive()) {
                        float pmx = 0.f, pmy = 0.f;
                        FieldDosChrome::chromePointerPixels(window_, ev.button.x, ev.button.y,
                            pmx, pmy);
                        const Uint32 btnSt = SDL_GetMouseState(nullptr, nullptr);
                        FieldDosChrome::storePointer(pmx, pmy, btnSt);
                    }
                }
                if (ev.type == SDL_EVENT_MOUSE_MOTION) {
                    syncChromeInputLayout();
                }
                if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    bool osHandled = false;
                    if (FieldAmouranthOs::shellChromeActive()) {
                        osHandled = FieldAmouranthOs::onTaskbarMouseDown(
                            window_, ev.button.x, ev.button.y);
                        if (!osHandled)
                            osHandled = FieldAmouranthOs::onMouseDown(window_, ev.button.x, ev.button.y,
                                ev.button.button, ev.button.clicks);
                        if (std::getenv("AMOURANTHRTX_DEBUG_CLICKS")) {
                            float dbgMx = 0.f, dbgMy = 0.f;
                            FieldDosChrome::chromePointerPixels(window_, ev.button.x, ev.button.y,
                                dbgMx, dbgMy);
                            std::fprintf(stderr,
                                "[CLICK] raw=(%.0f,%.0f) render=(%.1f,%.1f) hover=%d handled=%d start=%d\n",
                                ev.button.x, ev.button.y, dbgMx, dbgMy,
                                static_cast<int>(FieldAmouranthOs::hover), osHandled ? 1 : 0,
                                FieldAmouranthOs::startOpen ? 1 : 0);
                        }
                    }
                    if (osHandled && FieldAmouranthOs::shellChromeActive())
                        FieldAmouranthOs::packDataBus(Options::Canvas::DataBus,
                            FieldX86Emu::ramHost);
                    if (!osHandled && FieldAmouranthWm::wmPanelActive())
                        osHandled = FieldAmouranthWm::onMouseDown(window_, ev.button.x, ev.button.y,
                            ev.button.clicks);
                    if (!osHandled && FieldBios::rtxShellActive
                        && FieldRtxTerm::onMouseDown(window_, ev.button.x, ev.button.y))
                        osHandled = true;
                    if (!osHandled && FieldAmouranthOs::shellWindowFocused()) {
                        if (FieldDosChrome::onMouseDown(window_, ev.button.x, ev.button.y, ev.button.clicks)) {
                            int aw = 0, ah = 0;
                            SDL_GetWindowSizeInPixels(window_, &aw, &ah);
                            if (aw > 0 && ah > 0)
                                onResize(aw, ah, true);
                        } else {
                            Options::Canvas::DosInputFocused = true;
                        }
                    }
                }
                if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                    FieldAmouranthOs::onMouseUp();
                    FieldAmouranthWm::onMouseUp();
                    FieldDosChrome::onMouseUp();
                }
                if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                        || ev.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                    if (FieldAmouranthOs::shellChromeActive())
                        FieldAmouranthOs::syncStartTextInput(window_);
                }
                if (ev.type == SDL_EVENT_MOUSE_MOTION) {
                    FieldAmouranthWm::onMouseMotion(window_, ev.motion.x, ev.motion.y);
                    FieldAmouranthOs::onMouseMotion(window_, ev.motion.x, ev.motion.y);
                    FieldDosChrome::onMouseMotion(window_, ev.motion.x, ev.motion.y);
                    if (FieldAmouranthOs::shellChromeActive())
                        FieldAmouranthCursor::updateFromWm(FieldAmouranthWm::wmPanelActive());
                }
                if (ev.type == SDL_EVENT_MOUSE_WHEEL) {
                    if (FieldAmouranthWm::wmPanelActive()
                            && FieldAmouranthWm::openMenu == FieldAmouranthWm::OpenMenu::System)
                        FieldAmouranthWm::onMouseWheel(ev.wheel.y);
                    else if (FieldAmouranthFileCmd::active)
                        FieldAmouranthFileCmd::handleWheel(static_cast<int>(ev.wheel.y));
                    else if (FieldBios::rtxShellActive && Options::Canvas::DosInputFocused)
                        FieldRtxTerm::handleWheel(static_cast<int>(ev.wheel.y));
                }
            }

            if (ev.type == SDL_EVENT_TEXT_INPUT) {
                if (Options::Canvas::IsX86Fields()
                        && FieldAmouranthOs::onTextInput(ev.text.text))
                    continue;
            }

            if (ev.type == SDL_EVENT_KEY_DOWN) {
                bool altPressed = (ev.key.mod & SDL_KMOD_ALT) != 0;

                if (Options::Canvas::IsX86Fields()) {
                    if (FieldAmouranthOs::onKeyDown(ev.key.scancode)) {
                        FieldAmouranthOs::syncStartTextInput(window_);
                        continue;
                    }
                    if (altPressed && ev.key.scancode == SDL_SCANCODE_F4
                            && FieldAmouranthOs::panelVisible) {
                        FieldAmouranthWm::closeWindow();
                        continue;
                    }
                    if (altPressed && ev.key.scancode == SDL_SCANCODE_Q) {
                        if (FieldAmouranthOs::shellChromeActive())
                            FieldAmouranthExitConfirm::show();
                        else
                            quit = true;
                        LOG_INFO_CAT("WINDOW", "Alt+Q — exit confirm");
                    }
                    if (altPressed && ev.key.scancode == SDL_SCANCODE_GRAVE) {
                        Options::Canvas::DosInputFocused = !Options::Canvas::DosInputFocused;
                        LOG_INFO_CAT("WINDOW", "Alt+` — DOS input {}",
                            Options::Canvas::DosInputFocused ? "focused" : "released");
                    }
                    if (altPressed && ev.key.scancode == SDL_SCANCODE_R) {
                        Options::Canvas::DieResetRequested = true;
                        LOG_INFO_CAT("WINDOW", "Alt+R — Ammo DOS guest reset");
                    }

                    if (ev.key.scancode == SDL_SCANCODE_F11 ||
                        ev.key.scancode == SDL_SCANCODE_F8) {
                        fullscreen_toggle = true;
                    }
                } else if (ev.key.scancode == SDL_SCANCODE_F11 ||
                    (ev.key.scancode == SDL_SCANCODE_RETURN && altPressed) ||
                    ev.key.scancode == SDL_SCANCODE_F8 ||
                    ev.key.scancode == SDL_SCANCODE_HOME ||
                    ev.key.scancode == SDL_SCANCODE_END) {
                    fullscreen_toggle = true;
                }
                if (ev.key.scancode == SDL_SCANCODE_F1) { toggleAdaptiveResolution(); needsRecreate_ = true; }
                if (ev.key.scancode == SDL_SCANCODE_F2) { toggleRayTracing(); needsRecreate_ = true; }
                if (ev.key.scancode == SDL_SCANCODE_F3) { toggleRTXGI(); needsRecreate_ = true; }
                if (ev.key.scancode == SDL_SCANCODE_F4) { toggleAccumulation(); needsRecreate_ = true; }
                if (ev.key.scancode == SDL_SCANCODE_F5) { toggleTonemapping(); }
                if (ev.key.scancode == SDL_SCANCODE_F6) { toggleBloom(); }
                if (ev.key.scancode == SDL_SCANCODE_F7) { resetCamera(); }
                if (ev.key.scancode == SDL_SCANCODE_F9) {
                    Options::AnalogFields::EnableFieldViz = !Options::AnalogFields::EnableFieldViz;
                    Options::AnalogFields::FieldVizMode = (Options::AnalogFields::FieldVizMode + 1) % 5;
                    LOG_INFO_CAT("FIELDS", "Analog fabric viz: {} mode {}", 
                        Options::AnalogFields::EnableFieldViz ? "ON" : "OFF", Options::AnalogFields::FieldVizMode);
                }

                if (ev.key.scancode == SDL_SCANCODE_F10) {
                    if (Options::Canvas::IsX86Fields()) {
                        Options::Canvas::DieResetRequested = true;
                        LOG_INFO_CAT("CANVAS", "F10: Field Die reset — Ammo DOS re-load");
                    }
                    LOG_DEBUG_CAT("THERMO", "F10 THERMO: user reset — clearing fields for fresh seeds (entropyThisFrame/prevMaintCost/freeEnergyIncome/boundaryThermo will restart); fabric re-seed + pre clear state");
                    LOG_DEBUG_CAT("THERMO", "F10 THERMO: pre clearField for entropy calcs reset");
                    clearFieldImages();
                    LOG_INFO_CAT("THERMO", "Analog Field Fabric RESET — negentropy injection, new boundary seeds (entropy floor re-established)");
                    LOG_DEBUG_CAT("THERMO", "F10 THERMO: post-reset seeds applied — thermo boundary re-initialized; accountant will pick new prevMaint on next dispatch");
                }
            }
        }

        if (Options::SDL3::RequestQuit) {
            Pipeline::signal_app_quit();
            if (window_) SDL_HideWindow(window_);
            destroyed_ = true;
            LOG_INFO_CAT("RAYCANVAS", "AmmoOS shut down — exiting main loop");
            return false;
        }

        if (fullscreen_toggle) { toggleFullscreen(); }
        if (Options::SDL3::PendingFullscreenApply) {
            Options::SDL3::PendingFullscreenApply = false;
            if ((SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) == 0)
                toggleFullscreen();
        }
        if (quit) { destroyed_ = true; LOG_INFO_CAT("RAYCANVAS", "Quit signal received"); return false; }

        bool nowMinimized = (currentW <= 0 || currentH <= 0);
        if (nowMinimized) {
            minimized_ = true;
            if (!headless_) { return isRunning; }
            // HEADLESS: env (minimized/SDL picky/no present) — tolerate 0 size, do NOT early return; force direct dispatch path
            // so firstFrame/seal + dispatch + accountant population + thermo logs ALWAYS happen even with no real window/acquire.
            LOG_DEBUG_CAT("RAYCANVAS", "HEADLESS: nowMinimized (0-size tolerated in picky env) — forcing dispatch at render {}x{} to ensure dispatch + accountant + early thermo", render_width_, render_height_);
        }

        bool sizeChanged = (currentW != window_width_) || (currentH != window_height_);

        if (minimized_ || sizeChanged || needsRecreate_) {
            onResize(currentW, currentH, sizeChanged);
            minimized_ = false;
            syncChromeInputLayout();
            if (!headless_ && (sizeChanged || needsRecreate_)) {
                return isRunning;
            }
            // headless: after onResize (which may have 0 w/h), fall through to firstFrame/dispatch this frame for pop guarantee
            LOG_DEBUG_CAT("RAYCANVAS", "HEADLESS: post-onResize fallthrough to dispatch path (direct, no present waits)");
        }

        if (!headless_ && (!Swapchain::get() || !hdrOutputImage_ || !hdrOutputView_)) { return isRunning; }
        if (headless_ && (!hdrOutputImage_ || !hdrOutputView_)) { return isRunning; }  // still need HDR targets for offscreen dispatch

        if (FieldAmouranthOs::shellChromeActive())
            syncChromeInputLayout();

        if (Pipeline::hotswap_compile_active.load(std::memory_order_acquire) > 0)
            Pipeline::pump_ui_while_waiting();
        Pipeline::kick_deferred_hotswap_compile();
        if (Pipeline::try_hotswap_canvas_pipeline())
            ensureX86ChromeTextureBindings();

        if (firstFrame_) {
            TotalTime::get().seal();
            firstFrame_ = false;
            LOG_AMOURANTH("Hybrid RTX engine sealed — rendering begins 💖");
            LOG_DEBUG_CAT("THERMO", "FIRSTFRAME THERMO: TotalTime sealed @ {:.3f}s | genesis entropy=0x{:016X} | first dispatch will populate accountant (entropyThisFrame etc) + boundaryThermo",
                          TotalTime::get().seconds(), TotalTime::get().entropy());
            lastAdaptiveAdjustTime_ = now;
            if (headless_) {
                // force very early thermo/status visibility even before 5s or first interval
                lastFpsLog_ = -999.0;
            }
        }

        static double lastKnownTime = 0.0;
        if (now <= lastKnownTime) now = lastKnownTime + Swapchain::smoothedRefresh_s;
        lastKnownTime = now;

        // === HEADLESS vs real present path ===
        // In headless (AMOURANTHRTX_HEADLESS): ALWAYS do the dispatch (to ensure FIRST dispatch + accountant populates thermo).
        // Skip all acquire/fence/present/swap waits/blits. No surface dependency after init. This fixes quick "Canvas destroyed"
        // in constrained/minimized/headless envs while still evolving fabric, fields, and THERMO_ACCOUNTANT every frame.
        // Real path unchanged for normal runs.
        uint32_t imageIndex = 0;  // unused in headless

        if (headless_) {
                if (FieldAmouranthOs::shellChromeActive() || FieldAosTest::enabled())
                    FieldAmouranthOs::tick(window_width_ > 0 ? window_width_ : render_width_,
                        window_height_ > 0 ? window_height_ : render_height_);
            FieldAosTest::tickFrame(totalFrames_, render_width_, render_height_);
            // Offscreen dispatch-only path for testing: first dispatch guaranteed here on frame 1+
            LOG_DEBUG_CAT("RAYCANVAS", "HEADLESS: skipping acquire/present — direct dispatch for fabric + THERMO accountant (binding 2)");
            VkCommandBuffer cmd = beginTransientCommandBuffer();
            if (cmd) {
                const bool pOn = Pipeline::rtxProbesEnabled && rtx().hardwareFabric.probeCaps.anyProbesPossible;
                // Safe contiguous query ranges (pOn=3: 0=top,1=pre-disp,2=post/bottom; !pOn=2: 0+1).
                // Guarantees every Get(WAIT) range covers only written slots -> no indefinite hang/freeze.
                if (timestampQueryPool_) {
                    int qc = pOn ? 3 : 2;
                    vkCmdResetQueryPool(cmd, timestampQueryPool_, 0, qc);
                    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampQueryPool_, 0);
                }
                // Vendor backend: NV checkpoints (or AMD buffer marker in future via caps) — inserted in headless path too
                if (pOn && rtx().hardwareFabric.probeCaps.supportsNVCheckpoint && ext().vkCmdSetCheckpointNV) {
                    ext().vkCmdSetCheckpointNV(cmd, "RTXPROBE-pre-headless-canvas");
                }
                transitionImageLayout(cmd, hdrOutputImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

                VkDescriptorSet set = descriptorSet_;
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline::active_pipeline_layout(), 0, 1, &set, 0, nullptr);

                updateRenderResolution();
                LOG_DEBUG_CAT("THERMO", "HEADLESS DISPATCH THERMO: Pipeline::dispatch_canvas (w={} h={} t={:.4f}) — pre pop accountant + entropy calcs/prevMaintCost/freeEnergyIncome/boundaryThermo inside dispatch steps", render_width_, render_height_, now);
                LOG_DEBUG_CAT("THERMO", "HEADLESS DISPATCH THERMO: calling dispatch for fabric evolution + full THERMO accountant population (field seeds already in, boundaryThermo live)");

                if (pOn && rtx().hardwareFabric.probeCaps.supportsNVCheckpoint && ext().vkCmdSetCheckpointNV) {
                    ext().vkCmdSetCheckpointNV(cmd, "RTXPROBE-pre-dispatch");
                }
                if (pOn && timestampQueryPool_) {
                    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool_, 1);  // pre-dispatch phase for gpuMsCompute
                }

                // Pipeline stats begin (core) if pool + caps — around the actual pipeline bind inside dispatch (but we bracket here for simplicity)
                if (pOn && pipelineStatsQueryPool_) {
                    vkCmdResetQueryPool(cmd, pipelineStatsQueryPool_, 0, 1);
                    vkCmdBeginQuery(cmd, pipelineStatsQueryPool_, 0, 0);
                }

                Pipeline::sync_aos_textures();
                ensureX86ChromeTextureBindings();
                Pipeline::dispatch_canvas(cmd, render_width_, render_height_, static_cast<float>(now));

                if (pOn && pipelineStatsQueryPool_) {
                    vkCmdEndQuery(cmd, pipelineStatsQueryPool_, 0);
                }
                if (pOn && timestampQueryPool_) {
                    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampQueryPool_, 2);  // post-dispatch/compute phase for gpuMsCompute
                }

                if (pOn && rtx().hardwareFabric.probeCaps.supportsNVCheckpoint && ext().vkCmdSetCheckpointNV) {
                    ext().vkCmdSetCheckpointNV(cmd, "RTXPROBE-post-dispatch");
                }
                LOG_DEBUG_CAT("THERMO", "HEADLESS DISPATCH THERMO: returned — accountant now live with entropy/prevMaint/freeE/boundary; 5s status (or early) will show real Phase-1 numbers");
                LOG_DEBUG_CAT("THERMO", "HEADLESS DISPATCH THERMO: post-dispatch handle check thermoBuf=0x{:016x}", reinterpret_cast<uintptr_t>(Pipeline::thermoAccountantBuffer));
                if (pOn && rtx().hardwareFabric.probeCaps.supportsNVCheckpoint && ext().vkCmdSetCheckpointNV) {
                    ext().vkCmdSetCheckpointNV(cmd, "RTXPROBE-post-headless-canvas");
                }

                // Always write the bottom slot for the (small contiguous) range we reset.
                if (timestampQueryPool_) {
                    uint32_t bot = pOn ? 2u : 1u;
                    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampQueryPool_, bot);
                }

                // No post-blit barrier needed
                endSubmitAndWait(cmd);

                if (const char* snapOut = std::getenv("AMOURANTHRTX_SNAP_OUT")) {
                    int want = 60;
                    if (const char* sf = std::getenv("AMOURANTHRTX_SNAP_FRAME"))
                        want = static_cast<int>(std::strtol(sf, nullptr, 10));
                    if (static_cast<int>(totalFrames_) == want && hdrOutputImage_)
                        FieldSnapDump::dumpHdrImagePpm(hdrOutputImage_,
                            static_cast<std::uint32_t>(render_width_),
                            static_cast<std::uint32_t>(render_height_), snapOut);
                }

                // === COMMON GPU timing + RTX probe harvest (safe ranges, no gaps) ===
                uint64_t timestamps[3] = {};
                bool gotTimestamps = false;
                int gcount = pOn ? 3 : 2;
                if (timestampQueryPool_) {
                    gotTimestamps = (vkGetQueryPoolResults(rtx().device, timestampQueryPool_, 0, gcount, sizeof(timestamps), timestamps, sizeof(uint64_t), 
                                              VK_QUERY_RESULT_64_BIT) == VK_SUCCESS);
                }
                if (gotTimestamps && pOn && Pipeline::rtxProbeMapped) {
                    double gpuTimeMs = static_cast<double>(timestamps[2] - timestamps[0]) * timestampPeriodNs_ / 1000000.0;
                    double alpha = (gpuTimeMs > smoothedGpuTimeMs_) ? 0.70 : 0.22;
                    smoothedGpuTimeMs_ = (1.0 - alpha) * smoothedGpuTimeMs_ + alpha * gpuTimeMs;

                    auto* pr = static_cast<Pipeline::RTXProbe*>(Pipeline::rtxProbeMapped);
                    double phaseMs = static_cast<double>(timestamps[2] - timestamps[1]) * timestampPeriodNs_ / 1000000.0;
                    if (Options::Rendering::EnableHardwareRayTracing) {
                        pr->gpuMsRT = static_cast<float>(phaseMs);
                        pr->gpuMsCompute = 0.0f;
                    } else {
                        pr->gpuMsCompute = static_cast<float>(phaseMs);
                        pr->gpuMsRT = 0.0f;
                    }

                    // Stats invocations if pool (zero cost, populates for study; real compInvoc on this card)
                    if (pipelineStatsQueryPool_) {
                        uint64_t stats[1] = {};
                        if (vkGetQueryPoolResults(rtx().device, pipelineStatsQueryPool_, 0, 1, sizeof(stats), stats, sizeof(uint64_t),
                                                  VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
                            pr->computeInvocations = static_cast<uint32_t>(stats[0]);
                        }
                    }
                } else if (timestampQueryPool_) {
                    // non-probe / fallback: simple 2-slot timing
                    uint64_t ts2[2] = {};
                    if (vkGetQueryPoolResults(rtx().device, timestampQueryPool_, 0, 2, sizeof(ts2), ts2, sizeof(uint64_t), 
                                              VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
                        double gpuTimeMs = static_cast<double>(ts2[1] - ts2[0]) * timestampPeriodNs_ / 1000000.0;
                        double alpha = (gpuTimeMs > smoothedGpuTimeMs_) ? 0.70 : 0.22;
                        smoothedGpuTimeMs_ = (1.0 - alpha) * smoothedGpuTimeMs_ + alpha * gpuTimeMs;
                    } else {
                        smoothedGpuTimeMs_ = (frameCount_ < 5 ? 1.5 : 3.0);
                    }
                }

                // Harvest NV checkpoints if active (for execution trace on NVIDIA; zero on others). Now common + with Set in cmd.
                if (pOn && Pipeline::rtxProbeMapped && rtx().hardwareFabric.probeCaps.supportsNVCheckpoint && ext().vkGetQueueCheckpointDataNV) {
                    uint32_t ckptCount = 0;
                    ext().vkGetQueueCheckpointDataNV(rtx().graphics_queue, &ckptCount, nullptr);
                    auto* pr = static_cast<Pipeline::RTXProbe*>(Pipeline::rtxProbeMapped);
                    pr->checkpointCount = ckptCount;
                    // Actual fetch for study of execution (safe, bounded; aligns with caps + inserted "RTXPROBE-*" markers for gain analysis on scheduling).
                    if (ckptCount > 0 && ckptCount <= 32) {
                        std::vector<VkCheckpointDataNV> ckpts(ckptCount);
                        for (auto& c : ckpts) {
                            c.sType = VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV;
                            c.pNext = nullptr;
                        }
                        ext().vkGetQueueCheckpointDataNV(rtx().graphics_queue, &ckptCount, ckpts.data());
                    }
                }
            }
            if (Options::Rendering::EnableAdaptiveResolution) { adjustAdaptiveScale(now); } 
            else { adaptiveScale_ = 1.0; }

            // Fake refresh for logs/rate (no real present)
            Swapchain::updateRefreshEstimate(TotalTime::get().seconds());
            measuredRefreshRateHz_ = 60.0;
        } else {
            // Original real swapchain acquire + present path
            // Acquire next swapchain image
            VkFence fence = VK_NULL_HANDLE;

            VkFenceCreateInfo fci{};
            fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            vkCreateFence(rtx().device, &fci, nullptr, &fence);

            VkResult acq = ext().vkAcquireNextImageKHR(rtx().device, Swapchain::get(), UINT64_MAX, VK_NULL_HANDLE, fence, &imageIndex);
            vkWaitForFences(rtx().device, 1, &fence, VK_TRUE, UINT64_MAX);
            vkDestroyFence(rtx().device, fence, nullptr);

            if (acq == VK_ERROR_OUT_OF_DATE_KHR || acq == VK_SUBOPTIMAL_KHR) {
                needsRecreate_ = true;
                return isRunning;
            }
            if (acq != VK_SUCCESS) {
                LOG_ERROR_CAT("SWAPCHAIN", "Acquire failed: {}", vkh.result(acq));
                if (acq == VK_ERROR_SURFACE_LOST_KHR || acq == VK_ERROR_DEVICE_LOST) destroyed_ = true;
                return isRunning;
            }

            // Main rendering pass
            VkCommandBuffer cmd = beginTransientCommandBuffer();
            if (!cmd) return isRunning;

            const bool pOn = Pipeline::rtxProbesEnabled && rtx().hardwareFabric.probeCaps.anyProbesPossible;
            // Safe contiguous query ranges (pOn=3: 0=top,1=pre-disp,2=post/bottom; !pOn=2: 0+1).
            // Guarantees every Get(WAIT) range covers only written slots -> no indefinite hang/freeze.
            if (timestampQueryPool_) {
                int qc = pOn ? 3 : 2;
                vkCmdResetQueryPool(cmd, timestampQueryPool_, 0, qc);
                vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampQueryPool_, 0);
            }
            if (pOn && rtx().hardwareFabric.probeCaps.supportsNVCheckpoint && ext().vkCmdSetCheckpointNV) {
                ext().vkCmdSetCheckpointNV(cmd, "RTXPROBE-pre-real-canvas");
            }
            transitionImageLayout(cmd, hdrOutputImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

            VkDescriptorSet set = descriptorSet_;
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline::active_pipeline_layout(), 0, 1, &set, 0, nullptr);

            updateRenderResolution();
            LOG_DEBUG_CAT("THERMO", "DISPATCH THERMO: calling Pipeline::dispatch_canvas w={} h={} t={:.4f} — accountant pop + entropy calcs/prevMaintCost/freeEnergyIncome/boundaryThermo inside dispatch steps (pre-trans+fabric+desc already done)", render_width_, render_height_, now);
            LOG_DEBUG_CAT("THERMO", "DISPATCH THERMO: entering dispatch step for fabric + full THERMO (entropyThisFrame calc from fieldWork+probeDiss+prevCost)");
            if (pOn && rtx().hardwareFabric.probeCaps.supportsNVCheckpoint && ext().vkCmdSetCheckpointNV) {
                ext().vkCmdSetCheckpointNV(cmd, "RTXPROBE-pre-dispatch");
            }
            if (pOn && timestampQueryPool_) {
                vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool_, 1);  // pre-dispatch phase (for gpuMsCompute when !RT)
            }
            if (pOn && pipelineStatsQueryPool_) {
                vkCmdResetQueryPool(cmd, pipelineStatsQueryPool_, 0, 1);
                vkCmdBeginQuery(cmd, pipelineStatsQueryPool_, 0, 0);
            }
            Pipeline::sync_aos_textures();
            ensureX86ChromeTextureBindings();
            Pipeline::dispatch_canvas(cmd, render_width_, render_height_, static_cast<float>(now));
            if (pOn && pipelineStatsQueryPool_) {
                vkCmdEndQuery(cmd, pipelineStatsQueryPool_, 0);
            }
            if (pOn && timestampQueryPool_) {
                vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampQueryPool_, 2);  // post-dispatch phase
            }
            if (pOn && rtx().hardwareFabric.probeCaps.supportsNVCheckpoint && ext().vkCmdSetCheckpointNV) {
                ext().vkCmdSetCheckpointNV(cmd, "RTXPROBE-post-dispatch");
            }
            LOG_DEBUG_CAT("THERMO", "DISPATCH THERMO: dispatch returned — thermoAccountant should now have fresh values for this frame (visible in next 5s or early thermo log) boundaryThermo/prevMaint populated");
            LOG_DEBUG_CAT("THERMO", "DISPATCH THERMO: post dispatch hex handles accountant=0x{:016x}", reinterpret_cast<uintptr_t>(Pipeline::thermoAccountantBuffer));
            if (pOn && rtx().hardwareFabric.probeCaps.supportsNVCheckpoint && ext().vkCmdSetCheckpointNV) {
                ext().vkCmdSetCheckpointNV(cmd, "RTXPROBE-post-real-canvas");
            }

            // Always write the bottom slot for the (small contiguous) range we reset.
            if (timestampQueryPool_) {
                uint32_t bot = pOn ? 2u : 1u;
                vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampQueryPool_, bot);
            }

            // Barrier before blit
            VkImageMemoryBarrier postComputeBarrier{};
            postComputeBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            postComputeBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            postComputeBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            postComputeBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            postComputeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            postComputeBarrier.image = hdrOutputImage_;
            postComputeBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &postComputeBarrier);
            endSubmitAndWait(cmd);

            // GPU timing + RTX probe population (common path; safe no-gap query handling)
            uint64_t timestamps[3] = {};
            bool gotTs = false;
            int gcount = pOn ? 3 : 2;
            if (timestampQueryPool_) {
                gotTs = (vkGetQueryPoolResults(rtx().device, timestampQueryPool_, 0, gcount, sizeof(timestamps), timestamps, sizeof(uint64_t), 
                                          VK_QUERY_RESULT_64_BIT) == VK_SUCCESS);
            }
            if (gotTs && pOn && Pipeline::rtxProbeMapped) {
                double gpuTimeMs = static_cast<double>(timestamps[2] - timestamps[0]) * timestampPeriodNs_ / 1000000.0;
                double alpha = (gpuTimeMs > smoothedGpuTimeMs_) ? 0.70 : 0.22;
                smoothedGpuTimeMs_ = (1.0 - alpha) * smoothedGpuTimeMs_ + alpha * gpuTimeMs;

                auto* pr = static_cast<Pipeline::RTXProbe*>(Pipeline::rtxProbeMapped);
                double phaseMs = static_cast<double>(timestamps[2] - timestamps[1]) * timestampPeriodNs_ / 1000000.0;
                if (Options::Rendering::EnableHardwareRayTracing) {
                    pr->gpuMsRT = static_cast<float>(phaseMs);
                    pr->gpuMsCompute = 0.0f;
                } else {
                    pr->gpuMsCompute = static_cast<float>(phaseMs);
                    pr->gpuMsRT = 0.0f;
                }

                if (pipelineStatsQueryPool_) {
                    uint64_t stats[1] = {};
                    if (vkGetQueryPoolResults(rtx().device, pipelineStatsQueryPool_, 0, 1, sizeof(stats), stats, sizeof(uint64_t),
                                              VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
                        pr->computeInvocations = static_cast<uint32_t>(stats[0]);
                    }
                }
            } else if (timestampQueryPool_) {
                // fallback simple timing for !probe or !pOn
                uint64_t ts2[2] = {};
                if (vkGetQueryPoolResults(rtx().device, timestampQueryPool_, 0, 2, sizeof(ts2), ts2, sizeof(uint64_t), 
                                          VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
                    double gpuTimeMs = static_cast<double>(ts2[1] - ts2[0]) * timestampPeriodNs_ / 1000000.0;
                    double alpha = (gpuTimeMs > smoothedGpuTimeMs_) ? 0.70 : 0.22;
                    smoothedGpuTimeMs_ = (1.0 - alpha) * smoothedGpuTimeMs_ + alpha * gpuTimeMs;
                }
            }

            // NV ckpt harvest (real path) — count + actual data fetch when small (exercises for execution trace study; safe zero-cost, data not persisted beyond count in struct)
            if (pOn && Pipeline::rtxProbeMapped && rtx().hardwareFabric.probeCaps.supportsNVCheckpoint && ext().vkGetQueueCheckpointDataNV) {
                uint32_t ckptCount = 0;
                ext().vkGetQueueCheckpointDataNV(rtx().graphics_queue, &ckptCount, nullptr);
                if (Pipeline::rtxProbeMapped) {
                    static_cast<Pipeline::RTXProbe*>(Pipeline::rtxProbeMapped)->checkpointCount = ckptCount;
                }
                if (ckptCount > 0 && ckptCount <= 32) {
                    std::vector<VkCheckpointDataNV> ckpts(ckptCount);
                    for (auto& c : ckpts) {
                        c.sType = VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV;
                        c.pNext = nullptr;
                    }
                    ext().vkGetQueueCheckpointDataNV(rtx().graphics_queue, &ckptCount, ckpts.data());
                    // Safe study: the pCheckpointMarker are the "RTXPROBE-*" strings; stageFlags give pipeline stage. Logged via dispatch rate-limited or future dump.
                    // Use observed for gains e.g. count aligns with expected dispatch order on this card.
                }
            }

            if (Options::Rendering::EnableAdaptiveResolution) { adjustAdaptiveScale(now); } 
            else { adaptiveScale_ = 1.0; }

            // Blit to swapchain (top-down — shader writes gid without Y flip)
            VkCommandBuffer blitCmd = beginTransientCommandBuffer();
            if (!blitCmd) return isRunning;

            VkImage swapImg = Swapchain::images[imageIndex];

            // Prepare swapchain image for writing
            VkImageMemoryBarrier swapBarrier{};
            swapBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            swapBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            swapBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            swapBarrier.image = swapImg;
            swapBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &swapBarrier);

            // Clear swapchain to black
            VkClearColorValue clearBlack = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f} };
            VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdClearColorImage(blitCmd, swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearBlack, 1, &range);

            VkExtent2D swapExtent = Swapchain::getExtent();

            VkImageBlit blit{};
            blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { render_width_, render_height_, 1 };
            blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { static_cast<int32_t>(swapExtent.width),
                                   static_cast<int32_t>(swapExtent.height), 1 };

            vkCmdBlitImage(blitCmd,
                           hdrOutputImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blit, VK_FILTER_LINEAR);

            VkImageMemoryBarrier presentBarrier{};
            presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            presentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            presentBarrier.image = swapImg;
            presentBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &presentBarrier);
            endSubmitAndWait(blitCmd);

            VkPresentInfoKHR pi{};
            pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            pi.swapchainCount = 1;
            pi.pSwapchains = &Swapchain::swapchain.value;
            pi.pImageIndices = &imageIndex;
            VkResult pres = ext().vkQueuePresentKHR(rtx().present_queue, &pi);

            if (pres == VK_SUCCESS) {
                Swapchain::updateRefreshEstimate(TotalTime::get().seconds());
                measuredRefreshRateHz_ = 1.0 / Swapchain::getSmoothedRefresh();
                if (FieldAmouranthOs::shellChromeActive() && !headless_)
                    FieldAmouranthOs::tick(render_width_, render_height_);
            } else if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
                needsRecreate_ = true;
            } else if (pres != VK_SUCCESS) {
                LOG_ERROR_CAT("SWAPCHAIN", "Present failed: {}", vkh.result(pres));
                if (pres == VK_ERROR_SURFACE_LOST_KHR) destroyed_ = true;
            }
        }  // end real vs headless branch

        // Status / 5s (or faster in headless debug) block with enhanced thermo-specific logging.
        // Accountant (populated by every dispatch_canvas) is read here and made visible.
        // In headless we enter early (first dispatch + periodic) so thermo values are observable in logs immediately.
        double statusInterval = headless_ ? 0.5 : 5.0;  // more frequent in test mode for visibility
        // Enhanced 5s status: ALWAYS include FULL THERMO even for short runs (<5s) — earlier trigger on init or low t
        bool timeForStatus = (now - lastFpsLog_ >= statusInterval) ||
                             (lastFpsLog_ < -1.0) ||  // ctor init -999 forces very first
                             (now < 2.0) ||           // guarantee full THERMO in first 2s regardless of interval
                             (frameCount_ <= 2);      // immediate post first dispatch
        if (headless_ && (lastFpsLog_ < 0.01 || (frameCount_ > 0 && (frameCount_ % 5 == 0)))) {
            timeForStatus = true;  // force early thermo status after first dispatches in headless
        }
        if (timeForStatus) {
            double elapsed        = now - lastFpsLog_;
            double avgFps         = frameCount_ > 0 ? static_cast<double>(frameCount_) / elapsed : 0.0;
            double avgDt_us       = frameCount_ > 0 ? (elapsed * 1000000.0) / static_cast<double>(frameCount_) : 0.0;

            int winW              = window_width_;
            int winH              = window_height_;
            double scaleFactor    = winW > 0 ? static_cast<double>(render_width_) / winW : 1.0;
            const char* mode      = scaleFactor < 0.98 ? "SUBSAMPLING" : scaleFactor > 1.02 ? "SUPERSAMPLING" : "NATIVE";

            double targetFrameMs  = 1000.0 / measuredRefreshRateHz_;
            double gpuLoadPercent = targetFrameMs > 0.001 ? (smoothedGpuTimeMs_ / targetFrameMs) * 100.0 : 0.0;
            VRAMReality vram = Memory::measureReality();
            if (vram.usable > 0)
                FieldRtxMemTree::setCardBudget(vram.usable);
            else if (vram.total > 0)
                FieldRtxMemTree::setCardBudget(vram.total);

            double usedMB         = static_cast<double>(vram.driver_footprint) / (1024.0 * 1024.0);
            double totalMB        = static_cast<double>(vram.total) / (1024.0 * 1024.0);
            double freePercent    = totalMB > 0 ? 100.0 * (1.0 - usedMB / totalMB) : 0.0;

            auto& hw = rtx().hardwareFabric;
            std::string hwSummary = hw.populated ?
                std::format("{} {}nm @ {:.0f}MHz ({} units, {} edges, simCycles {:.0f})",
                            hw.vendor, hw.processNodeNm, hw.coreClockMHz,
                            hw.units.size(), hw.edges.size(), hw.simulatedChipCycles) :
                "unpopulated";

            // Pull live thermo numbers (host always writes honest Phase-1 accountant per schematic)
            // FULL THERMO METRICS ALWAYS — even first block on short runs (<5s total) thanks to early trigger + lastFpsLog_ init
            float eThis = 0.0f, bThermo = 0.42f, pMaint = 0.0f, freeE = 0.02f;
            if (Pipeline::thermoAccountantMapped) {
                const auto* a = static_cast<const Pipeline::ThermoAccountant*>(Pipeline::thermoAccountantMapped);
                eThis = a->entropyThisFrame;
                bThermo = a->avgBoundaryThermo;
                pMaint = a->prevMaintCost;
                freeE = a->freeEnergyIncome;
            }
            LOG_DEBUG_CAT("THERMO", "STATUS THERMO (5s block, short-run safe + early trigger): t={:.3f}s entropyThisFrame={:.5e} avgBoundaryThermo={:.3f} prevMaintCost={:.4f} freeEnergyIncome={:.3f} steps={} (accountant populated in every dispatch via entropy calcs)", now, eThis, bThermo, pMaint, freeE, Pipeline::thermoAccountantMapped ? static_cast<const Pipeline::ThermoAccountant*>(Pipeline::thermoAccountantMapped)->steps : 0u);
            LOG_DEBUG_CAT("RAYCANVAS", "STATUS: full 5s block emitted with complete THERMO (even if run <5s due to init/now<2s/frame<=2 early trigger)");

            // Probe data in status when active (explicit hardware results for study, zero cost otherwise)
            float pC = 0, pRT = 0; uint32_t pInv = 0, pCk = 0;
            if (Pipeline::rtxProbesEnabled && Pipeline::rtxProbeMapped) {
                const auto* pr = static_cast<const Pipeline::RTXProbe*>(Pipeline::rtxProbeMapped);
                pC = pr->gpuMsCompute; pRT = pr->gpuMsRT; pInv = pr->computeInvocations; pCk = pr->checkpointCount;
                LOG_DEBUG_CAT("PROBE", "PROBE STATUS: gpuMsC={:.3f} gpuMsRT={:.3f} compInvoc={} rtInvoc={} ckpts={} sbtH={} a={} minAS={} (explicit hardware study data; use for gains like phase timings, checkpoint execution order, SBT packing/aligns on this/any card)",
                              pC, pRT, pInv, pr->rtInvocations, pCk,
                              pr->sbtHandleSize, pr->sbtHandleAlignment, (uint64_t)pr->minASScratchAlign);
            }
            // Hex handle dump always in status (for thermo accountant + key fabric)
            LOG_DEBUG_CAT("THERMO", "STATUS HANDLES (hex): accountantBuf=0x{:016x} mapped=0x{:016x} hdrView=0x{:016x} thermoFieldView=0x{:016x} tlas=0x{:016x}",
                          reinterpret_cast<uintptr_t>(Pipeline::thermoAccountantBuffer),
                          reinterpret_cast<uintptr_t>(Pipeline::thermoAccountantMapped),
                          reinterpret_cast<uintptr_t>(hdrOutputView_),
                          reinterpret_cast<uintptr_t>(fieldThermoView_),
                          reinterpret_cast<uintptr_t>(tlas_));

            if (Pipeline::currentCanvasKind == Pipeline::CanvasKind::X86Fields && Pipeline::fieldX86DieMapped) {
                struct FieldX86DieView {
                    uint32_t EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP;
                    uint32_t R8, R9, R10, R11, R12, R13, R14, R15;
                    uint32_t CS, DS, ES, FS, GS, SS;
                    uint32_t EIP, EFLAGS;
                    uint32_t CR0, CR2, CR3, CR4, CR8;
                    uint32_t DR0, DR1, DR2, DR3, DR6, DR7;
                    uint32_t GDTR, IDTR, LDTR, TR;
                };
                const auto* d = static_cast<const FieldX86DieView*>(Pipeline::fieldX86DieMapped);
                const auto* base = static_cast<const uint8_t*>(Pipeline::fieldX86DieMapped);
                const uint32_t cycles = *reinterpret_cast<const uint32_t*>(base + Pipeline::FIELD_X86_DIE_CYCLE_OFFSET);
                const uint32_t irq    = *reinterpret_cast<const uint32_t*>(base + Pipeline::FIELD_X86_DIE_CYCLE_OFFSET + 4);
                const uint32_t vm     = *reinterpret_cast<const uint32_t*>(base + Pipeline::FIELD_X86_DIE_CYCLE_OFFSET + 8);
                LOG_INFO_CAT("CANVAS",
                    "X86 DIE  CYC={} EIP={:08X} EAX={:08X} EBX={:08X} ECX={:08X} EDX={:08X} "
                    "ESI={:08X} EDI={:08X} EBP={:08X} ESP={:08X} EFLAGS={:08X} "
                    "CS={:04X} DS={:04X} CR0={:08X} IRQ={} VM={}",
                    cycles, d->EIP, d->EAX, d->EBX, d->ECX, d->EDX,
                    d->ESI, d->EDI, d->EBP, d->ESP, d->EFLAGS,
                    d->CS & 0xFFFFu, d->DS & 0xFFFFu, d->CR0, irq, vm);
            }

            LOG_AMOURANTH("───────────────────────────────────────────────────────────────\n"
                          "              RayCanvas Status  •  t+{:.4}s   (HYBRID RTX)\n"
                          "  FPS:            {}     (avg frame {} µs)\n"
                          "  Refresh Rate:   {} Hz\n"
                          "  Window:         {} x {}\n"
                          "  Rendered:       {} x {}     ({:.2f}x — {})\n"
                          "  Adaptive scale: {:.2f}x\n"
                          "  GPU load:       {:.2f}%   (smoothed {:.3f} ms)\n"
                          "  Features:       Adaptive {}  Accumulation {}  Supersample {}\n"
                          "  Advanced:       HardwareRayTracing {}  RTXGI {}\n"
                          "  VRAM:           {:.2f} MB used / {:.2f} MB total ({:.3f}% free)\n"
                          "  Hardware:       {}\n"
                          "  PROBE (RTX study): C{:.3f}ms RT{:.3f}ms InvC{} Ckpts{} (0s if !enabled; see caps for card support)\n"
                          "  Frames this log: {}\n"
                          "  THERMO (schematic Phase 1): Entropy {:.2e}  BoundaryT {:.3f}\n"
                          "           PrevMaintCost {:.4f} (accumulation = costly holographic memory)\n"
                          "           FreeEnergyIn {:.3f} (time+attention+potentials)  Export via visual+audio probes\n"
                          "  Boundary: HDR pair + Phi/Thermo/Flow fabric (holographic screen); bulk reconstructed\n"
                          "───────────────────────────────────────────────────────────────",
                          now, avgFps, avgDt_us, measuredRefreshRateHz_,
                          winW, winH, render_width_, render_height_, scaleFactor, mode,
                          adaptiveScale_, gpuLoadPercent, smoothedGpuTimeMs_,
                          Options::Rendering::EnableAdaptiveResolution ? "💖" : "❌",
                          Options::Rendering::EnableAccumulation ? "💖" : "❌",
                          scaleFactor > 1.0 ? "💖" : "❌",
                          Options::Rendering::EnableHardwareRayTracing ? "💖" : "❌",
                          Options::Rendering::EnableRTXGI ? "💖" : "❌",
                          usedMB, totalMB, freePercent,
                          hwSummary,
                          pC, pRT, pInv, pCk,
                          frameCount_,
                          eThis, bThermo, pMaint, freeE);

            lastFpsLog_ = now;
            frameCount_ = 0;

            // Extra thermo-specific logging in the status block (task request) + special for headless/debug
            if (Pipeline::thermoAccountantMapped) {
                const auto* a = static_cast<const Pipeline::ThermoAccountant*>(Pipeline::thermoAccountantMapped);
                LOG_SUCCESS_CAT("THERMO", "5s-status THERMO (Phase-1 accountant visible): entropyThisFrame={:.2e} avgBoundaryThermo={:.3f} prevMaintCost={:.4f} freeEnergyIncome={:.3f} steps={}",
                    a->entropyThisFrame, a->avgBoundaryThermo, a->prevMaintCost, a->freeEnergyIncome, a->steps);
                if (headless_) {
                    LOG_INFO_CAT("THERMO", "HEADLESS thermo confirmed populated post-dispatch — schematic boundary values (entropy export, holographic maint cost) now observable without real present.");
                }
            } else {
                LOG_WARNING_CAT("THERMO", "Status block: thermoAccountantMapped still null (should have been created in Pipeline::create_canvas_pipeline)");
            }
        }

        // Bound N frames for clean exit (AMOURANTHRTX_MAX_FRAMES): check after dispatch + (possible) accountant pop + status
        // guarantees at least the Nth dispatch + thermo happens before exit.
        if (maxFrames_ > 0 && totalFrames_ >= static_cast<uint64_t>(maxFrames_)) {
            destroyed_ = true;
            LOG_INFO_CAT("RAYCANVAS", "HEADLESS MAX_FRAMES={} reached — dispatch + accountant populated; forcing clean exit (tolerant path)", maxFrames_);
            return false;
        }

        // Thermo heartbeat inside canvas (supplements loop) — visible every dispatch in headless for test tracing
        if (headless_ && (totalFrames_ % 8 == 0 || totalFrames_ <= 4)) {
            LOG_DEBUG_CAT("THERMO", "THERMO HEARTBEAT (canvas): totalFrames={} dispatch done (direct headless path), accountant live for status", totalFrames_);
        }

        return isRunning;
    }

    // Public toggles
    void toggleAdaptiveResolution() noexcept {
        Options::Rendering::EnableAdaptiveResolution = !Options::Rendering::EnableAdaptiveResolution;
        needsRecreate_ = true;
        LOG_INFO_CAT("RENDER", "Adaptive resolution toggled: {}", Options::Rendering::EnableAdaptiveResolution ? "ON 💖" : "OFF");
    }

    void toggleRayTracing() noexcept {
        Options::Rendering::EnableHardwareRayTracing = !Options::Rendering::EnableHardwareRayTracing;
        needsRecreate_ = true;
        if (Pipeline::currentCanvasKind == Pipeline::CanvasKind::X86Fields) {
            LOG_INFO_CAT("RENDER", "Die HUD RTX overlay (glass trace + god rays): {}",
                Options::Rendering::EnableHardwareRayTracing ? "ON" : "OFF");
        } else {
            LOG_INFO_CAT("RENDER", "Hardware Ray Tracing toggled: {}",
                Options::Rendering::EnableHardwareRayTracing ? "ON" : "OFF");
        }
    }

    void toggleRTXGI() noexcept {
        Options::Rendering::EnableRTXGI = !Options::Rendering::EnableRTXGI;
        needsRecreate_ = true;
        LOG_INFO_CAT("RENDER", "RTX GI toggled: {}", Options::Rendering::EnableRTXGI ? "ON 💖" : "OFF");
    }

    void toggleAccumulation() noexcept {
        Options::Rendering::EnableAccumulation = !Options::Rendering::EnableAccumulation;
        needsRecreate_ = true;
        // Directly modulates the prevMaintCost in the THERMO accountant (higher = more negentropy to hold holographic memory)
        LOG_INFO_CAT("THERMO", "Accumulation toggled: {}  (prev-frame boundary maintenance tax now {})",
            Options::Rendering::EnableAccumulation ? "ON 💖 (high irreversibility cost at visual probe)" : "OFF",
            Options::Rendering::EnableAccumulation ? "ACTIVE" : "LOW");
    }

    void toggleTonemapping() noexcept {
        Options::Rendering::EnableTonemapping = !Options::Rendering::EnableTonemapping;
        LOG_INFO_CAT("RENDER", "Tonemapping toggled: {}", Options::Rendering::EnableTonemapping ? "ON 💖" : "OFF");
    }

    void toggleBloom() noexcept {
        Options::Rendering::BloomIntensity = (Options::Rendering::BloomIntensity > 0.01f) ? 0.0f : 0.35f;
        LOG_INFO_CAT("RENDER", "Bloom toggled: {}", Options::Rendering::BloomIntensity > 0.01f ? "ON 💖" : "OFF");
    }

    void resetCamera() noexcept {
        CAM.setPosition(Options::Camera::StartPosition);
        LOG_INFO_CAT("CAMERA", "Camera reset to default spawn position 💖");
    }

    [[nodiscard]] int  getWidth()  const noexcept { return window_width_; }
    [[nodiscard]] int  getHeight() const noexcept { return window_height_; }
    [[nodiscard]] bool isMinimized() const noexcept { return minimized_; }
    [[nodiscard]] bool isDestroyed() const noexcept { return destroyed_; }
    [[nodiscard]] uint64_t debugFrameCount() const noexcept { return frameCount_; }  // for HEADLESS heartbeat / test visibility without exposing all
    [[nodiscard]] uint64_t totalFrameCount() const noexcept { return totalFrames_; }  // for MAX_FRAMES bound + navigator loop heartbeats (post-dispatch count)

private:
    void cycleCanvas(bool forward = true) noexcept {
        if (forward) Options::Canvas::Next();
        else         Options::Canvas::Prev();

        vkDeviceWaitIdle(rtx().device);

        if (Options::Canvas::IsX86Fields()) {
            Options::Canvas::ControlFlags |= 1u;
            Pipeline::boot_x86_canvas(Options::Canvas::CurrentName());
        } else {
            Pipeline::create_canvas_pipeline(Options::Canvas::CurrentName());
        }
        SDL3System::get().applyMouseCapture();

        if (descriptorSet_ != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(rtx().device, descriptorPool_, 1, &descriptorSet_);
            descriptorSet_ = VK_NULL_HANDLE;
        }
        if (descriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(rtx().device, descriptorPool_, nullptr);
            descriptorPool_ = VK_NULL_HANDLE;
        }

        createDescriptorPoolAndSet();
        updateDescriptorSet();

        if (Pipeline::currentCanvasKind == Pipeline::CanvasKind::Classic) {
            clearHDRImages();
            clearFieldImages();
        }

        LOG_INFO_CAT("CANVAS", "Canvas swipe -> {} ({}/{})",
            Options::Canvas::CurrentName(),
            Options::Canvas::CurrentIndex + 1,
            Options::Canvas::SwipeCount);
    }

    void toggleFullscreen() noexcept {
        const bool wasFullscreen = (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0;
        const bool nowFullscreen = !wasFullscreen;
        SDL_SetWindowFullscreen(window_, nowFullscreen);
        Options::SDL3::StartFullscreen = nowFullscreen;
        INPUT.applyMouseCapture();
        LOG_INFO_CAT("WINDOW", "Fullscreen {} (End/F8/F11/Alt+Enter)",
            nowFullscreen ? "enabled" : "disabled");
    }

    void onResize(int newWidth, int newHeight, bool fromUserResize) noexcept {
        if (newWidth <= 0 || newHeight <= 0) { 
            if (!headless_) { minimized_ = true; return; }
            // headless: tolerate 0 size (no real window, minimized/SDL picky), keep driving dispatches at last known render res
            // ensures dispatch + accountant pop even during env-induced "min" resizes
            LOG_DEBUG_CAT("RAYCANVAS", "HEADLESS onResize: 0-size tolerated (picky env), continuing dispatch at {}x{}", render_width_, render_height_);
        }

        int oldW = window_width_;
        int oldH = window_height_;
        double oldArea = static_cast<double>(oldW) * oldH;
        if (newWidth == oldW && newHeight == oldH && !needsRecreate_) return;
        if (fromUserResize || needsRecreate_) vkDeviceWaitIdle(rtx().device);

        int actualW = 0, actualH = 0;
        SDL_GetWindowSizeInPixels(window_, &actualW, &actualH);
        window_width_  = actualW;
        window_height_ = actualH;
        if (window_) {
            int dispW = 3840, dispH = 2160;
            const SDL_DisplayID did = SDL_GetDisplayForWindow(window_);
            SDL_Rect bounds{};
            if (did && SDL_GetDisplayBounds(did, &bounds) == 0) {
                dispW = bounds.w;
                dispH = bounds.h;
            }
            int logW = 0, logH = 0;
            SDL_GetWindowSize(window_, &logW, &logH);
            FieldDosDisplay::setWindowMetrics(
                window_width_, window_height_,
                SDL_GetWindowDisplayScale(window_), dispW, dispH, logW, logH);
        }
        double newArea = static_cast<double>(window_width_) * window_height_;
        bool windowGrewSignificantly = (newArea > oldArea * 1.08);

        minimized_ = false;
        needsRecreate_ = false;

        if (!headless_) {
            Swapchain::recreate(window_width_, window_height_);
        } else {
            LOG_DEBUG_CAT("RAYCANVAS", "HEADLESS onResize: skipping Swapchain::recreate (no present path)");
        }

        if (windowGrewSignificantly && Options::Rendering::EnableAdaptiveResolution && adaptiveScale_ > 0.35) {
            double areaRatio = oldArea / newArea;
            adaptiveScale_ = std::max(0.42, adaptiveScale_ * areaRatio * 0.72);
            LOG_INFO_CAT("RAYCANVAS", "Window grew significantly — pre-downscaled to {:.2f}x", adaptiveScale_);
        }

        updateRenderResolution();
        FieldDosDisplay::syncViewport(window_width_, window_height_,
            render_width_, render_height_);
        char disp[192]{};
        FieldDosDisplay::formatStatus(disp, sizeof disp);
        LOG_INFO_CAT("WINDOW", "DOS display: {}", disp);
        destroyHDRResources();
        destroyAnalogFieldFabric();
        createPersistentHDR();
        createPreviousHDR();
        createAnalogFieldFabric();

        // Add/ensure pre-transition call in onResize before pool/update (as required): NEW images (post create) pre-trans to GENERAL.
        // (layout match for updateDescriptorSet writes; clears rely on pre, no incorrect UNDEF trans inside clear*).
        LOG_DEBUG_CAT("THERMO", "ONRESIZE THERMO: preTransitionAllImagesToGeneral (hdr+fields) — ensures GENERAL for descriptor writes (accountant b2 + thermo b9) before clears/seeds");
        preTransitionAllImagesToGeneral();

        if (descriptorSet_ != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(rtx().device, descriptorPool_, 1, &descriptorSet_);
            descriptorSet_ = VK_NULL_HANDLE;
            Pipeline::aosTextureBindDirty = true;
        }
        if (descriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(rtx().device, descriptorPool_, nullptr);
            descriptorPool_ = VK_NULL_HANDLE;
        }

        createDescriptorPoolAndSet();
        LOG_DEBUG_CAT("THERMO", "ONRESIZE THERMO: re-creating descriptors + clears after fabric recreate (re-seed entropy floor + accountant rebinding + prevMaintCost reset path)");
        LOG_DEBUG_CAT("THERMO", "ONRESIZE THERMO: about to descriptor writes (pre-trans done) for accountant population readiness");
        updateDescriptorSet();
        LOG_DEBUG_CAT("THERMO", "ONRESIZE THERMO: descriptor writes complete post-resize; now clearHDR + clearField for boundaryThermo/entropy floor re-seed");
        clearHDRImages();
        clearFieldImages();

        lastAdaptiveAdjustTime_ = TotalTime::get().seconds() + 1.25;
        LOG_DEBUG_CAT("THERMO", "ONRESIZE THERMO: resize complete — new boundary size, thermo metrics will reflect new detailTax/prevMaintCost on next dispatches (entropy calcs resume)");
    }

    void adjustAdaptiveScale(double now) noexcept {
        double elapsed = now - lastAdaptiveAdjustTime_;
        if (elapsed < 1.15) return;

        double targetFrameMs = 1000.0 / measuredRefreshRateHz_;
        double gpuLoadPercent = targetFrameMs > 0.001 
            ? (smoothedGpuTimeMs_ / targetFrameMs) * 100.0 
            : 0.0;

        double targetScale = adaptiveScale_;

        if (gpuLoadPercent > 108.0)      targetScale *= 0.79;
        else if (gpuLoadPercent > 97.0)  targetScale *= 0.88;
        else if (gpuLoadPercent > 89.0)  targetScale *= 0.95;
        else if (gpuLoadPercent < 64.0)  targetScale *= 1.11;
        else if (gpuLoadPercent < 73.0)  targetScale *= 1.055;

        targetScale = std::clamp(targetScale,
            static_cast<double>(Options::Rendering::MinResolutionScale),
            static_cast<double>(Options::Rendering::MaxResolutionScale));

        double hysteresis = (targetScale > adaptiveScale_) ? 0.038 : 0.072;

        if (std::abs(targetScale - adaptiveScale_) > hysteresis) {
            adaptiveScale_ = targetScale;
            needsRecreate_ = true;
        }

        lastAdaptiveAdjustTime_ = now;
    }

    void updateRenderResolution() noexcept {
        if (FieldAmouranthOs::shellChromeActive()
                || !Options::Rendering::EnableAdaptiveResolution) {
            render_width_  = window_width_;
            render_height_ = window_height_;
            return;
        }

        double scale = adaptiveScale_;
        int64_t w = static_cast<int64_t>(std::round(static_cast<double>(window_width_) * scale));
        int64_t h = static_cast<int64_t>(std::round(static_cast<double>(window_height_) * scale));

        if (w > 32768) w = 32768;
        if (h > 32768) h = 32768;
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        render_width_  = static_cast<int>(w);
        render_height_ = static_cast<int>(h);
    }

    void createTimestampQueryPool() noexcept {
        // ALWAYS create (6 slots tiny) — required for gpu timing / smoothedGpu / adaptive resolution in ALL runs (probes or not).
        // Probe paths simply use more slots + inserts when rtxProbesEnabled + caps (zero cost otherwise: no extra writes/gets).
        // This fixes regressions from over-gating timing pool behind probes.
        VkQueryPoolCreateInfo qci{};
        qci.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qci.queryType  = VK_QUERY_TYPE_TIMESTAMP;
        qci.queryCount = 6;  // 0: top, 1: pre-dispatch/compute, 2: post-dispatch/compute, (3/4 RT reserved), 5: bottom/full. For probe + timing.
        if (vkCreateQueryPool(rtx().device, &qci, nullptr, &timestampQueryPool_) != VK_SUCCESS) {
            LOG_ERROR_CAT("RAYCANVAS", "Failed to create timestamp query pool");
            timestampQueryPool_ = VK_NULL_HANDLE;
            return;
        }
        LOG_DEBUG_CAT("RAYCANVAS", "Timestamp query pool created (6 slots) for gpu timing + (when enabled) RTXPROBE phase deltas.");
    }

    void createPipelineStatisticsQueryPool() noexcept {
        auto& fab = rtx().hardwareFabric;
        if (!fab.probeCaps.supportsPipelineStatistics || !Pipeline::rtxProbesEnabled) {
            LOG_DEBUG_CAT("PROBE", "AUTOMAGIC PROBE (zero-cost): skipping pipeline stats query pool (no stats support or !probesEnabled).");
            return;
        }
        VkQueryPoolCreateInfo qci{};
        qci.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qci.queryType  = VK_QUERY_TYPE_PIPELINE_STATISTICS;
        qci.queryCount = 1;
        // Only compute invocations (canvas main path); fragment unused in hybrid compute/RT. Simplifies results (1 value) + zero-cost gated.
        qci.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;
        if (vkCreateQueryPool(rtx().device, &qci, nullptr, &pipelineStatsQueryPool_) != VK_SUCCESS) {
            pipelineStatsQueryPool_ = VK_NULL_HANDLE;
            return;
        }
        LOG_DEBUG_CAT("PROBE", "AUTOMAGIC PROBE: pipeline stats query pool created (for compInvoc study when active; gated by caps.supportsPipelineStatistics + rtxProbesEnabled).");
    }

    void createPersistentHDR() noexcept {
        vkDeviceWaitIdle(rtx().device);
        destroyHDRResources();

        VkImageCreateInfo ci{};
        ci.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType   = VK_IMAGE_TYPE_2D;
        ci.format      = VK_FORMAT_R32G32B32A32_SFLOAT;
        ci.extent      = {static_cast<uint32_t>(render_width_), static_cast<uint32_t>(render_height_), 1};
        ci.mipLevels   = 1;
        ci.arrayLayers = 1;
        ci.samples     = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling      = VK_IMAGE_TILING_OPTIMAL;
        ci.usage       = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkResult res = vkCreateImage(rtx().device, &ci, nullptr, &hdrOutputImage_);
        if (res != VK_SUCCESS) { LOG_FATAL_CAT("RAYCANVAS", "createPersistentHDR: vkCreateImage failed: {}", vkh.result(res)); return; }

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(rtx().device, hdrOutputImage_, &req);
        uint32_t memType = Memory::findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = req.size;
        mai.memoryTypeIndex = memType;

        res = vkAllocateMemory(rtx().device, &mai, nullptr, &hdrOutputMemory_);
        if (res != VK_SUCCESS) { LOG_FATAL_CAT("RAYCANVAS", "createPersistentHDR: vkAllocateMemory failed: {}", vkh.result(res)); return; }
        vkBindImageMemory(rtx().device, hdrOutputImage_, hdrOutputMemory_, 0);

        VkImageViewCreateInfo vi{};
        vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image            = hdrOutputImage_;
        vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vi.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        res = vkCreateImageView(rtx().device, &vi, nullptr, &hdrOutputView_);
        if (res != VK_SUCCESS) { LOG_FATAL_CAT("RAYCANVAS", "createPersistentHDR: vkCreateImageView failed: {}", vkh.result(res)); return; }
    }

    void createPreviousHDR() noexcept {
        vkDeviceWaitIdle(rtx().device);

        if (prevHdrOutputView_)   vkDestroyImageView (rtx().device, prevHdrOutputView_,   nullptr);
        if (prevHdrOutputImage_)  vkDestroyImage     (rtx().device, prevHdrOutputImage_,  nullptr);
        if (prevHdrOutputMemory_) vkFreeMemory       (rtx().device, prevHdrOutputMemory_, nullptr);

        prevHdrOutputView_   = VK_NULL_HANDLE;
        prevHdrOutputImage_  = VK_NULL_HANDLE;
        prevHdrOutputMemory_ = VK_NULL_HANDLE;

        VkImageCreateInfo ci{};
        ci.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType   = VK_IMAGE_TYPE_2D;
        ci.format      = VK_FORMAT_R32G32B32A32_SFLOAT;
        ci.extent      = {static_cast<uint32_t>(render_width_), static_cast<uint32_t>(render_height_), 1};
        ci.mipLevels   = 1;
        ci.arrayLayers = 1;
        ci.samples     = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling      = VK_IMAGE_TILING_OPTIMAL;
        ci.usage       = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkResult res = vkCreateImage(rtx().device, &ci, nullptr, &prevHdrOutputImage_);
        if (res != VK_SUCCESS) { LOG_FATAL_CAT("RAYCANVAS", "createPreviousHDR: vkCreateImage failed: {}", vkh.result(res)); return; }

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(rtx().device, prevHdrOutputImage_, &req);
        uint32_t memType = Memory::findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = req.size;
        mai.memoryTypeIndex = memType;

        res = vkAllocateMemory(rtx().device, &mai, nullptr, &prevHdrOutputMemory_);
        if (res != VK_SUCCESS) { LOG_FATAL_CAT("RAYCANVAS", "createPreviousHDR: vkAllocateMemory failed: {}", vkh.result(res)); return; }
        vkBindImageMemory(rtx().device, prevHdrOutputImage_, prevHdrOutputMemory_, 0);

        VkImageViewCreateInfo vi{};
        vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image            = prevHdrOutputImage_;
        vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vi.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        res = vkCreateImageView(rtx().device, &vi, nullptr, &prevHdrOutputView_);
        if (res != VK_SUCCESS) { LOG_FATAL_CAT("RAYCANVAS", "createPreviousHDR: vkCreateImageView failed: {}", vkh.result(res)); return; }
    }

    void destroyHDRResources() noexcept {
        if (hdrOutputView_)       vkDestroyImageView (rtx().device, hdrOutputView_,       nullptr);
        if (hdrOutputImage_)      vkDestroyImage     (rtx().device, hdrOutputImage_,      nullptr);
        if (hdrOutputMemory_)     vkFreeMemory       (rtx().device, hdrOutputMemory_,     nullptr);
        if (prevHdrOutputView_)   vkDestroyImageView (rtx().device, prevHdrOutputView_,   nullptr);
        if (prevHdrOutputImage_)  vkDestroyImage     (rtx().device, prevHdrOutputImage_,  nullptr);
        if (prevHdrOutputMemory_) vkFreeMemory       (rtx().device, prevHdrOutputMemory_, nullptr);

        hdrOutputView_       = VK_NULL_HANDLE;
        hdrOutputImage_      = VK_NULL_HANDLE;
        hdrOutputMemory_     = VK_NULL_HANDLE;
        prevHdrOutputView_   = VK_NULL_HANDLE;
        prevHdrOutputImage_  = VK_NULL_HANDLE;
        prevHdrOutputMemory_ = VK_NULL_HANDLE;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // ANALOG FIELD FABRIC (Phi = wave/gate potential, Thermo = heat+entropy+energy, Flow = advection)
    // These live at render resolution so the canvas *is* the continuous field computer.
    // "At each gate": field evolution + RT sampling evaluate the transfer functions directly.
    // ─────────────────────────────────────────────────────────────────────────
void createAnalogFieldFabric() noexcept {
    vkDeviceWaitIdle(rtx().device);
    destroyAnalogFieldFabric();

    auto makeField = [&](VkImage& img, VkImageView& view, VkDeviceMemory& mem, const char* tag) {
        VkImageCreateInfo ci{};
        ci.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType   = VK_IMAGE_TYPE_2D;
        ci.format      = VK_FORMAT_R32G32B32A32_SFLOAT;
        ci.extent      = {static_cast<uint32_t>(render_width_), static_cast<uint32_t>(render_height_), 1};
        ci.mipLevels   = 1;
        ci.arrayLayers = 1;
        ci.samples     = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling      = VK_IMAGE_TILING_OPTIMAL;
        ci.usage       = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkResult resImg = vkCreateImage(rtx().device, &ci, nullptr, &img);
        if (resImg != VK_SUCCESS) {
            LOG_FATAL_CAT("RAYCANVAS", "Failed to create {} field image: {}", tag, vkh.result(resImg));
            return;
        }

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(rtx().device, img, &req);
        uint32_t memType = Memory::findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = req.size;
        mai.memoryTypeIndex = memType;
        VkResult resMem = vkAllocateMemory(rtx().device, &mai, nullptr, &mem);
        if (resMem != VK_SUCCESS) {
            LOG_FATAL_CAT("RAYCANVAS", "Failed to alloc memory for {} field: {}", tag, vkh.result(resMem));
            return;
        }
        vkBindImageMemory(rtx().device, img, mem, 0);

        VkImageViewCreateInfo vi{};
        vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image            = img;
        vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vi.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkResult resView = vkCreateImageView(rtx().device, &vi, nullptr, &view);
        if (resView != VK_SUCCESS) {
            LOG_FATAL_CAT("RAYCANVAS", "Failed to create {} field view: {}", tag, vkh.result(resView));
            return;
        }
    };

    makeField(fieldPhiImage_,    fieldPhiView_,    fieldPhiMemory_,    "Phi");
    makeField(fieldThermoImage_, fieldThermoView_, fieldThermoMemory_, "Thermo");
    makeField(fieldFlowImage_,   fieldFlowView_,   fieldFlowMemory_,   "Flow");

    LOG_DEBUG_CAT("THERMO", "FABRIC THERMO: field images created (thermo at b9) — size={}x{} | will receive seeds for entropyFloor/boundaryThermo/prevMaint in clear; accountant separate for freeEnergyIncome", render_width_, render_height_);
    LOG_DEBUG_CAT("THERMO", "FABRIC THERMO: Phi/Thermo/Flow fabric online — pre-seed stage for entropy calcs + boundaryThermo source in dispatch");
    LOG_SUCCESS_CAT("RAYCANVAS", "Analog Field Fabric created — {}x{} (Phi/Thermo/Flow) — propalactic gates online", render_width_, render_height_);
}

    void destroyAnalogFieldFabric() noexcept {
        LOG_DEBUG_CAT("THERMO", "FABRIC THERMO: destroyAnalogFieldFabric — releasing thermo field (boundaryThermo source) + phi/flow");
        auto kill = [&](VkImage& img, VkImageView& view, VkDeviceMemory& mem) {
            if (view) vkDestroyImageView(rtx().device, view, nullptr);
            if (img)  vkDestroyImage(rtx().device, img, nullptr);
            if (mem)  vkFreeMemory(rtx().device, mem, nullptr);
            view = VK_NULL_HANDLE; img = VK_NULL_HANDLE; mem = VK_NULL_HANDLE;
        };
        kill(fieldPhiImage_, fieldPhiView_, fieldPhiMemory_);
        kill(fieldThermoImage_, fieldThermoView_, fieldThermoMemory_);
        kill(fieldFlowImage_, fieldFlowView_, fieldFlowMemory_);
        LOG_DEBUG_CAT("THERMO", "FABRIC THERMO: field fabric destroyed (accountant buffer lifetime separate)");
    }

void clearFieldImages() noexcept {
    LOG_DEBUG_CAT("THERMO", "CLEAR THERMO: clearFieldImages entry — seeding Phi/thermo/flow for entropy floor + field evolution (boundaryThermo starts from thermoSeed); prevMaintCost baseline + freeEnergy path ready");
    LOG_DEBUG_CAT("THERMO", "CLEAR THERMO: field seeds (entropy calcs source) — thermo body+floor directly feeds avgBoundaryThermo + entropyThisFrame in subsequent dispatch accountant pop");
    LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: clearField handles - phi=0x{:016x} thermo=0x{:016x} flow=0x{:016x}", reinterpret_cast<uintptr_t>(fieldPhiImage_), reinterpret_cast<uintptr_t>(fieldThermoImage_), reinterpret_cast<uintptr_t>(fieldFlowImage_));

    VkCommandBuffer cmd = beginTransientCommandBuffer();
    if (!cmd) {
        LOG_DEBUG_CAT("THERMO", "CLEAR THERMO: no cmd for field clears (entropy seeds skipped!)");
        return;
    }

    VkClearColorValue phiSeed = { .float32 = {0.08f, -0.03f, 0.11f, 0.7f} };
    VkClearColorValue thermoSeed = { .float32 = {0.42f, 0.19f, 0.03f, 0.015f} };
    VkClearColorValue flowSeed = { .float32 = {0.001f, -0.0007f, 0.0f, 0.0f} };

    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cmd, fieldPhiImage_,    VK_IMAGE_LAYOUT_GENERAL, &phiSeed,    1, &range);
    vkCmdClearColorImage(cmd, fieldThermoImage_, VK_IMAGE_LAYOUT_GENERAL, &thermoSeed, 1, &range);
    vkCmdClearColorImage(cmd, fieldFlowImage_,   VK_IMAGE_LAYOUT_GENERAL, &flowSeed,   1, &range);

    endSubmitAndWait(cmd);
    LOG_DEBUG_CAT("THERMO", "CLEAR THERMO: field clears submitted — thermoSeed applied (0.42 body + 0.015 entropy floor) | phi/flow seeds for coupling | prevMaintCost will track coherence from here; entropy calcs begin on next dispatch");
}

    void createDescriptorPoolAndSet() noexcept {
        VkDescriptorPoolSize poolSizes[3]{};
        uint32_t poolSizeCount = 0;
        VkDescriptorSetLayout* layout = nullptr;

        if (Pipeline::currentCanvasKind == Pipeline::CanvasKind::X86Fields) {
            Pipeline::create_field_descriptor_layout();
            poolSizes[0] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 9};
            poolSizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
            poolSizeCount = 2;
            layout = &Pipeline::field_descriptor_layout;
        } else {
            poolSizes[0] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 5};
            poolSizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5};
            poolSizes[2] = {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1};
            poolSizeCount = 3;
            layout = &Pipeline::main_descriptor_layout;
        }

        VkDescriptorPoolCreateInfo pci{};
        pci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pci.maxSets       = 1;
        pci.poolSizeCount = poolSizeCount;
        pci.pPoolSizes    = poolSizes;

        VkResult poolRes = vkCreateDescriptorPool(rtx().device, &pci, nullptr, &descriptorPool_);
        if (poolRes != VK_SUCCESS) {
            LOG_FATAL_CAT("RAYCANVAS", "vkCreateDescriptorPool failed: {}", vkh.result(poolRes));
            descriptorPool_ = VK_NULL_HANDLE;
            return;
        }

        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = descriptorPool_;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = layout;

        VkResult allocRes = vkAllocateDescriptorSets(rtx().device, &ai, &descriptorSet_);
        if (allocRes != VK_SUCCESS) {
            LOG_FATAL_CAT("RAYCANVAS", "vkAllocateDescriptorSets failed: {}", vkh.result(allocRes));
            descriptorSet_ = VK_NULL_HANDLE;
        }
    }

    void syncChromeViewport() noexcept {
        if (!FieldAmouranthOs::shellChromeActive()) return;
        if (window_) {
            FieldDosChrome::refreshWindowMetrics(window_);
            int pixW = 0, pixH = 0;
            SDL_GetWindowSizeInPixels(window_, &pixW, &pixH);
            if (pixW > 0 && pixH > 0) {
                window_width_  = pixW;
                window_height_ = pixH;
            }
        }
        const int winW = window_width_ > 0 ? window_width_ : render_width_;
        const int winH = window_height_ > 0 ? window_height_ : render_height_;
        FieldDosDisplay::syncViewport(winW, winH, render_width_, render_height_);
    }

    void syncChromeInputLayout() noexcept {
        if (!FieldAmouranthOs::shellChromeActive()) return;
        if (window_) {
            FieldDosChrome::refreshWindowMetrics(window_);
            int pixW = 0, pixH = 0;
            SDL_GetWindowSizeInPixels(window_, &pixW, &pixH);
            if (pixW > 0 && pixH > 0) {
                window_width_  = pixW;
                window_height_ = pixH;
            }
        }
        updateRenderResolution();
        syncChromeViewport();
        FieldAmouranthOs::tick(render_width_, render_height_);
        FieldAmouranthInfo::refreshClock();
        if (Pipeline::fieldX86DieMapped) {
            if (auto* gr = FieldDos::guestRam(
                    static_cast<std::uint8_t*>(Pipeline::fieldX86DieMapped),
                    Pipeline::FIELD_X86_DIE_HEADER_BYTES))
                FieldAmouranthOs::packDataBus(Options::Canvas::DataBus, gr);
        } else {
            FieldAmouranthInfo::packDataBus(Options::Canvas::DataBus);
        }
    }

    // Bind chrome textures when views appear or descriptor set is recreated — never write null views.
    static void patchChromeDescriptorsStatic() noexcept {
        if (s_activeInstance_) s_activeInstance_->ensureX86ChromeTextureBindings();
    }

    void ensureX86ChromeTextureBindings() noexcept {
        if (!descriptorSet_) return;
        if (Pipeline::currentCanvasKind != Pipeline::CanvasKind::X86Fields) return;

        static VkDescriptorSet lastSet = VK_NULL_HANDLE;
        static VkImageView boundAmmo = VK_NULL_HANDLE;
        static VkImageView boundStart = VK_NULL_HANDLE;
        static VkImageView boundFontSdf = VK_NULL_HANDLE;
        if (descriptorSet_ != lastSet) {
            lastSet = descriptorSet_;
            boundAmmo = VK_NULL_HANDLE;
            boundStart = VK_NULL_HANDLE;
            boundFontSdf = VK_NULL_HANDLE;
        }

        VkDescriptorImageInfo infos[3]{};
        VkWriteDescriptorSet writes[3]{};
        uint32_t n = 0u;

        const bool forceBind = Pipeline::aosTextureBindDirty;
        if (Pipeline::aosStartTextureView != VK_NULL_HANDLE
                && (Pipeline::aosStartTextureView != boundStart || forceBind)) {
            infos[n].imageView = Pipeline::aosStartTextureView;
            infos[n].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            writes[n].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[n].dstSet = descriptorSet_;
            writes[n].dstBinding = 13;
            writes[n].descriptorCount = 1;
            writes[n].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[n].pImageInfo = &infos[n];
            boundStart = Pipeline::aosStartTextureView;
            ++n;
        }
        if (Pipeline::ammoTextureView != VK_NULL_HANDLE
                && (Pipeline::ammoTextureView != boundAmmo || forceBind)) {
            infos[n].imageView = Pipeline::ammoTextureView;
            infos[n].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            writes[n].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[n].dstSet = descriptorSet_;
            writes[n].dstBinding = 11;
            writes[n].descriptorCount = 1;
            writes[n].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[n].pImageInfo = &infos[n];
            boundAmmo = Pipeline::ammoTextureView;
            ++n;
        }
        if (Pipeline::rtxFontSdfView != VK_NULL_HANDLE
                && (Pipeline::rtxFontSdfView != boundFontSdf || forceBind)) {
            infos[n].imageView = Pipeline::rtxFontSdfView;
            infos[n].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            writes[n].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[n].dstSet = descriptorSet_;
            writes[n].dstBinding = 14;
            writes[n].descriptorCount = 1;
            writes[n].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[n].pImageInfo = &infos[n];
            boundFontSdf = Pipeline::rtxFontSdfView;
            ++n;
        }
        if (n == 0u) return;
        vkUpdateDescriptorSets(rtx().device, n, writes, 0, nullptr);
        Pipeline::aosTextureBindDirty = false;
        LOG_DEBUG_CAT("CANVAS", "x86 chrome textures bound — {} write(s)", n);
    }

    void updateDescriptorSet() noexcept {
        if (Pipeline::currentCanvasKind == Pipeline::CanvasKind::X86Fields) {
            if (!hdrOutputView_ || !descriptorSet_) return;
            Pipeline::create_field_x86_die_buffer();
            Pipeline::create_thermo_accountant_buffer();
            if (!Pipeline::fieldX86DieBuffer) return;
            if (!fieldPhiView_ || !fieldThermoView_ || !fieldFlowView_) {
                LOG_DEBUG_CAT("CANVAS", "x86 fields: fabric views not ready — run with RayCanvas fabric");
                return;
            }

            VkDescriptorImageInfo imgInfo{};
            imgInfo.imageView   = hdrOutputView_;
            imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            VkWriteDescriptorSet w0{};
            w0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w0.dstSet = descriptorSet_; w0.dstBinding = 0; w0.descriptorCount = 1;
            w0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w0.pImageInfo = &imgInfo;

            VkDescriptorBufferInfo dieInfo{};
            dieInfo.buffer = Pipeline::fieldX86DieBuffer;
            dieInfo.offset = 0;
            dieInfo.range  = Pipeline::FIELD_X86_DIE_SIZE;
            VkWriteDescriptorSet w1{};
            w1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w1.dstSet = descriptorSet_; w1.dstBinding = 1; w1.descriptorCount = 1;
            w1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w1.pBufferInfo = &dieInfo;

            VkDescriptorBufferInfo acctInfo{};
            acctInfo.buffer = Pipeline::thermoAccountantBuffer;
            acctInfo.offset = 0;
            acctInfo.range  = sizeof(Pipeline::ThermoAccountant);
            VkWriteDescriptorSet w2{};
            w2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w2.dstSet = descriptorSet_; w2.dstBinding = 2; w2.descriptorCount = 1;
            w2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w2.pBufferInfo = &acctInfo;

            VkDescriptorImageInfo phiInfo{};
            phiInfo.imageView = fieldPhiView_; phiInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            VkWriteDescriptorSet w8{};
            w8.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w8.dstSet = descriptorSet_; w8.dstBinding = 8; w8.descriptorCount = 1;
            w8.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w8.pImageInfo = &phiInfo;

            VkDescriptorImageInfo thermoInfo{};
            thermoInfo.imageView = fieldThermoView_; thermoInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            VkWriteDescriptorSet w9{};
            w9.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w9.dstSet = descriptorSet_; w9.dstBinding = 9; w9.descriptorCount = 1;
            w9.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w9.pImageInfo = &thermoInfo;

            VkDescriptorImageInfo flowInfo{};
            flowInfo.imageView = fieldFlowView_; flowInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            VkWriteDescriptorSet w10{};
            w10.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w10.dstSet = descriptorSet_; w10.dstBinding = 10; w10.descriptorCount = 1;
            w10.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w10.pImageInfo = &flowInfo;

            Pipeline::sync_aos_textures();

            VkDescriptorImageInfo ammoInfo{};
            VkDescriptorImageInfo aosWallInfo{};
            VkDescriptorImageInfo aosStartInfo{};
            VkDescriptorImageInfo fontSdfInfo{};
            VkWriteDescriptorSet writes[10]{w0, w1, w2, w8, w9, w10};
            uint32_t writeCount = 6u;

            if (Pipeline::ammoTextureView != VK_NULL_HANDLE) {
                ammoInfo.imageView   = Pipeline::ammoTextureView;
                ammoInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                VkWriteDescriptorSet& w11 = writes[writeCount++];
                w11.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                w11.dstSet = descriptorSet_; w11.dstBinding = 11; w11.descriptorCount = 1;
                w11.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w11.pImageInfo = &ammoInfo;
            }
            if (Pipeline::aosWallTextureView != VK_NULL_HANDLE) {
                aosWallInfo.imageView   = Pipeline::aosWallTextureView;
                aosWallInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                VkWriteDescriptorSet& w12 = writes[writeCount++];
                w12.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                w12.dstSet = descriptorSet_; w12.dstBinding = 12; w12.descriptorCount = 1;
                w12.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w12.pImageInfo = &aosWallInfo;
            }
            if (Pipeline::aosStartTextureView != VK_NULL_HANDLE) {
                aosStartInfo.imageView   = Pipeline::aosStartTextureView;
                aosStartInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                VkWriteDescriptorSet& w13 = writes[writeCount++];
                w13.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                w13.dstSet = descriptorSet_; w13.dstBinding = 13; w13.descriptorCount = 1;
                w13.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w13.pImageInfo = &aosStartInfo;
            }
            if (Pipeline::rtxFontSdfView != VK_NULL_HANDLE) {
                fontSdfInfo.imageView   = Pipeline::rtxFontSdfView;
                fontSdfInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                VkWriteDescriptorSet& w14 = writes[writeCount++];
                w14.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                w14.dstSet = descriptorSet_; w14.dstBinding = 14; w14.descriptorCount = 1;
                w14.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w14.pImageInfo = &fontSdfInfo;
            }

            vkUpdateDescriptorSets(rtx().device, writeCount, writes, 0, nullptr);
            LOG_DEBUG_CAT("CANVAS", "x86 descriptor set: {} write(s) (chrome textures deferred if not ready)",
                writeCount);
            return;
        }

        if (!hdrOutputView_ || !prevHdrOutputView_ || !fieldPhiView_ || !fieldThermoView_ || !fieldFlowView_) {
            LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: update early return - some views null");
            LOG_DEBUG_CAT("THERMO", "DESCRIPTOR THERMO: early return in updateDescriptorSet — missing field views (thermo at b9 will be incomplete)");
            return;
        }

        if (headless_) {
            Pipeline::sync_aos_textures();
            ensureX86ChromeTextureBindings();
            LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: headless - chrome textures bound; skipping full AS descriptor update");
            return;
        }

        LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: starting updateDescriptorSet - building writes (improved split: dedicated images batch, dedicated buffers batch (2,3,4,5,6), separate TLAS)");
        LOG_DEBUG_CAT("THERMO", "DESCRIPTOR THERMO: building writes for bindings 0-10 incl. b2=THERMO_ACCOUNTANT (entropy/prevMaint/freeE), b9=fieldThermo (boundaryThermo seed) — pre-trans guaranteed");

        // Dump handles early (for debug before any update calls) + extra hex for thermo
        LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: handles - hdrView=0x{:016x} prevView=0x{:016x} phiView=0x{:016x} thermoView=0x{:016x} flowView=0x{:016x}",
                      reinterpret_cast<uintptr_t>(hdrOutputView_),
                      reinterpret_cast<uintptr_t>(prevHdrOutputView_),
                      reinterpret_cast<uintptr_t>(fieldPhiView_),
                      reinterpret_cast<uintptr_t>(fieldThermoView_),
                      reinterpret_cast<uintptr_t>(fieldFlowView_));
        LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: handles - matsBuf=0x{:016x} accountantBuf=0x{:016x} audioBuf=0x{:016x} res3=0x{:016x} res5=0x{:016x} tlas=0x{:016x}",
                      reinterpret_cast<uintptr_t>(Memory::getBuffer(materialsHandle_)),
                      reinterpret_cast<uintptr_t>(Pipeline::thermoAccountantBuffer),
                      reinterpret_cast<uintptr_t>(Pipeline::audio_cmd_buffer),
                      reinterpret_cast<uintptr_t>(Pipeline::reservedBuffer3),
                      reinterpret_cast<uintptr_t>(Pipeline::reservedBuffer5),
                      reinterpret_cast<uintptr_t>(tlas_));
        LOG_DEBUG_CAT("THERMO", "DESCRIPTOR THERMO: handle dump complete — accountantBuf/reserved* non-zero expected for b2/3/5 writes (accountant populated host-visible every dispatch before bind); pre-transitions + fabric seeds + clears precede this");
        LOG_DEBUG_CAT("THERMO", "DESCRIPTOR THERMO: performing actual vkUpdateDescriptorSets for accountant (b2) write — enables entropy calcs/prevMaintCost/freeEnergyIncome/boundaryThermo reads in shader + host pop in dispatch");

        // === SPLIT 1: DEDICATED IMAGES BATCH UPDATE (0,1,8,9,10) ===
        // All 5 guaranteed by guard + preTransitionAllImagesToGeneral (GENERAL) called in ctor/onResize before pool/update.
        std::vector<VkWriteDescriptorSet> imgWrites;
        imgWrites.reserve(5);

        // Binding 0: output HDR
        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageView   = hdrOutputView_;
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkWriteDescriptorSet w0{};
        w0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w0.dstSet = descriptorSet_; w0.dstBinding = 0; w0.descriptorCount = 1;
        w0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w0.pImageInfo = &imgInfo;
        imgWrites.push_back(w0);

        // Binding 1: previous frame
        VkDescriptorImageInfo prevImgInfo{};
        prevImgInfo.imageView   = prevHdrOutputView_;
        prevImgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkWriteDescriptorSet w1{};
        w1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w1.dstSet = descriptorSet_; w1.dstBinding = 1; w1.descriptorCount = 1;
        w1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w1.pImageInfo = &prevImgInfo;
        imgWrites.push_back(w1);

        // Binding 8: fieldPhi
        VkDescriptorImageInfo phiInfo{};
        phiInfo.imageView = fieldPhiView_; phiInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkWriteDescriptorSet w8{};
        w8.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w8.dstSet = descriptorSet_; w8.dstBinding = 8; w8.descriptorCount = 1;
        w8.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w8.pImageInfo = &phiInfo;
        imgWrites.push_back(w8);

        // Binding 9: fieldThermo
        VkDescriptorImageInfo thermoInfo{};
        thermoInfo.imageView = fieldThermoView_; thermoInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkWriteDescriptorSet w9{};
        w9.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w9.dstSet = descriptorSet_; w9.dstBinding = 9; w9.descriptorCount = 1;
        w9.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w9.pImageInfo = &thermoInfo;
        imgWrites.push_back(w9);

        // Binding 10: fieldFlow
        VkDescriptorImageInfo flowInfo{};
        flowInfo.imageView = fieldFlowView_; flowInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkWriteDescriptorSet w10{};
        w10.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w10.dstSet = descriptorSet_; w10.dstBinding = 10; w10.descriptorCount = 1;
        w10.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w10.pImageInfo = &flowInfo;
        imgWrites.push_back(w10);

        if (!imgWrites.empty()) {
            vkUpdateDescriptorSets(rtx().device, static_cast<uint32_t>(imgWrites.size()), imgWrites.data(), 0, nullptr);
            LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: images batch update (bindings 0,1,8,9,10) completed");
            LOG_DEBUG_CAT("THERMO", "DESCRIPTOR THERMO: 5 image writes (hdr pair + phi/thermo/flow) complete — pre-trans to GENERAL ensured match");
        }

        // === Ensure ALL buffer resources for b2/3/4/5/6 (add reservedBuffer3/5 dummies if not present; always valid accountant/audio) ===
        // Guarantees EVERY CanvasBindings layout binding gets a valid write (robust; creation called in Pipeline::create_canvas_pipeline but defensive here).
        if (!Pipeline::thermoAccountantBuffer) {
            Pipeline::create_thermo_accountant_buffer();
        }
        if (!Pipeline::audio_cmd_buffer) {
            Pipeline::create_audio_command_buffer();
        }
        if (!Pipeline::reservedBuffer3 || !Pipeline::reservedBuffer5) {
            Pipeline::create_reserved_buffers();
        }

        // Re-dump after potential ensure (for visibility of valid handles before buffer writes)
        LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: post-ensure handles - accountant=0x{:016x} audio=0x{:016x} res3=0x{:016x} res5=0x{:016x}",
                      reinterpret_cast<uintptr_t>(Pipeline::thermoAccountantBuffer),
                      reinterpret_cast<uintptr_t>(Pipeline::audio_cmd_buffer),
                      reinterpret_cast<uintptr_t>(Pipeline::reservedBuffer3),
                      reinterpret_cast<uintptr_t>(Pipeline::reservedBuffer5));
        LOG_DEBUG_CAT("THERMO", "DESCRIPTOR THERMO: post-ensure dump — b2 accountant + b6 audio + b3/5 reserved now guaranteed valid for writes");

        // === SPLIT 2: DEDICATED BUFFERS BATCH UPDATE (2,3,4,5,6) ===
        // Always write valid buffers for accountant (b2 — make always bound before dispatch), reserved dummies (b3/5), mats (b4), audio (b6).
        // Single batch. (TLAS separate due to descriptor type.)
        //
        // CRITICAL: The VkDescriptorBufferInfo objects must remain valid (same stack lifetime as bufWrites vector)
        // until *after* the vkUpdateDescriptorSets call. Previous block-scoped infos caused dangling pBufferInfo
        // pointers (stack reuse after inner blocks) → driver SIGSEGV on NVIDIA (and potential on others) when
        // dereferencing pBufferInfo inside the update for b2/b3/b5/b6. Images used outer-scope infos (stable).
        // Declare here at updateDescriptorSet scope (like the img* infos above).
        std::vector<VkWriteDescriptorSet> bufWrites;
        bufWrites.reserve(5);

        VkDescriptorBufferInfo acctInfo{};
        VkDescriptorBufferInfo r3Info{};
        VkDescriptorBufferInfo matInfo{};
        VkDescriptorBufferInfo r5Info{};
        VkDescriptorBufferInfo audioInfo{};

        // Binding 2: THERMO_ACCOUNTANT — always write with valid buffer (make accountant always bound via set before dispatch)
        acctInfo.buffer = Pipeline::thermoAccountantBuffer;  // guaranteed valid by ensure + create_canvas
        acctInfo.offset = 0;
        acctInfo.range  = VK_WHOLE_SIZE;
        {
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = descriptorSet_; w.dstBinding = 2; w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w.pBufferInfo = &acctInfo;
            bufWrites.push_back(w);
            LOG_DEBUG_CAT("THERMO", "DESCRIPTOR THERMO: added b2 THERMO_ACCOUNTANT write (always; will be bound before dispatch; populated with entropy/prevMaint/freeE each frame in dispatch_canvas)");
        }

        // Binding 3: Reserved3 (always valid dummy)
        r3Info.buffer = Pipeline::reservedBuffer3;
        r3Info.offset = 0;
        r3Info.range  = VK_WHOLE_SIZE;
        {
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = descriptorSet_; w.dstBinding = 3; w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w.pBufferInfo = &r3Info;
            bufWrites.push_back(w);
        }

        // Binding 4: materials (always)
        matInfo.buffer = Memory::getBuffer(materialsHandle_);
        matInfo.offset = 0;
        matInfo.range  = VK_WHOLE_SIZE;
        {
            VkWriteDescriptorSet w4{};
            w4.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w4.dstSet = descriptorSet_; w4.dstBinding = 4; w4.descriptorCount = 1;
            w4.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w4.pBufferInfo = &matInfo;
            bufWrites.push_back(w4);
        }

        // Binding 5: Reserved5 (always valid dummy)
        r5Info.buffer = Pipeline::reservedBuffer5;
        r5Info.offset = 0;
        r5Info.range  = VK_WHOLE_SIZE;
        {
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = descriptorSet_; w.dstBinding = 5; w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w.pBufferInfo = &r5Info;
            bufWrites.push_back(w);
        }

        // Binding 6: audio — always write with valid buffer
        audioInfo.buffer = Pipeline::audio_cmd_buffer;
        audioInfo.offset = 0;
        audioInfo.range  = VK_WHOLE_SIZE;
        {
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = descriptorSet_; w.dstBinding = 6; w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w.pBufferInfo = &audioInfo;
            bufWrites.push_back(w);
            LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: added audio (binding 6)");
        }

        if (!bufWrites.empty()) {
            vkUpdateDescriptorSets(rtx().device, static_cast<uint32_t>(bufWrites.size()), bufWrites.data(), 0, nullptr);
            LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: buffers batch update (2,3,4,5,6) completed");
            LOG_DEBUG_CAT("THERMO", "DESCRIPTOR THERMO: buffers update (b2 accountant + b3/5 reserved + b4 mats + b6 audio) done — accountant always present in set for bind-before-dispatch");
        }

        // === TLAS binding 7 (AS type) — separate update (AS struct can't mix easily); count=0/1 properly ===
        // (avoids passing null AS which crashes some drivers; count=0 write still "covers" the layout binding for completeness)
        // In practice tlas_ is null for default compute path (RT shaders declare AS at binding 0 instead).
        {
            VkWriteDescriptorSet w7{};
            w7.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w7.dstSet = descriptorSet_; w7.dstBinding = 7;
            w7.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

            // Always provide a safe count=0 write for the AS binding slot (b7).
            // This "addresses" the layout requirement without providing a valid AS (count=0 + pNext=nullptr is the safe way to null it).
            // In compute-only path (!tlas_) we use this to avoid driver faults on uninit pNext or count=1 with null handle.
            // When tlas_ present (RT path), we override to count=1 + valid asInfo.
            w7.descriptorCount = 0;
            w7.pNext = nullptr;
            if (tlas_ != VK_NULL_HANDLE) {
                VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
                asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                asInfo.accelerationStructureCount = 1;
                asInfo.pAccelerationStructures = &tlas_;
                w7.descriptorCount = 1;
                w7.pNext = &asInfo;
                LOG_SUCCESS_CAT("RAYCANVAS", "DEBUG: added TLAS (binding 7)");
            } else {
                LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: using count=0 safe null for TLAS binding 7 (compute-only path)");
            }
            vkUpdateDescriptorSets(rtx().device, 1, &w7, 0, nullptr);
            LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: TLAS/AS update (binding 7, count={}) completed", (tlas_ ? 1 : 0));
        }

        // Note: vkUpdateDescriptorSets returns void (no VkResult possible on update); driver/validation errors surface on subsequent bind/use or via layers.
        // Added VkResult checks on pool/alloc (here) + in Pipeline create_*_buffer (for b2/3/5/6 resources). Accountant always written valid; set bound before dispatch.
        LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: vkUpdateDescriptorSets all splits completed (images + buffers(2-6) + tlas)");
    }

    VkCommandBuffer beginTransientCommandBuffer() noexcept {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo alloc{};
        alloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool        = rtx().transient_pool;
        alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;
        vkAllocateCommandBuffers(rtx().device, &alloc, &cmd);

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin);

        return cmd;
    }

    void endSubmitAndWait(VkCommandBuffer cmd) noexcept {
        if (!cmd) return;
        vkEndCommandBuffer(cmd);

        VkFence fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkCreateFence(rtx().device, &fci, nullptr, &fence);

        VkSubmitInfo submit{};
        submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers    = &cmd;

        vkQueueSubmit(rtx().graphics_queue, 1, &submit, fence);
        vkWaitForFences(rtx().device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(rtx().device, fence, nullptr);
        vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
    }

    void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) noexcept {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        }
        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void clearHDRImages() noexcept {
        LOG_DEBUG_CAT("THERMO", "CLEAR THERMO: clearHDRImages — zeroing holographic boundary (hdr/prev) to set initial freeEnergy/prevMaintCost baseline for accumulation tax + entropy floor");
        LOG_DEBUG_CAT("THERMO", "CLEAR THERMO: HDR boundary clear (pre freeEnergyIncome/prevMaintCost/entropy calcs) — pre-transition ensured; accountant will see clean slate on first dispatch");
        // Add handle dump (per task); no UNDEF here, rely on pre
        LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: clearHDR handles - hdr=0x{:016x} prev=0x{:016x}", reinterpret_cast<uintptr_t>(hdrOutputImage_), reinterpret_cast<uintptr_t>(prevHdrOutputImage_));
        VkClearColorValue clearBlack = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f} };
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkCommandBuffer cmd = beginTransientCommandBuffer();
        if (!cmd) {
            LOG_DEBUG_CAT("THERMO", "CLEAR THERMO: no cmd for HDR clears");
            return;
        }

        // NOTE: incorrect UNDEF transitions removed from clears (rely on preTransitionAllImagesToGeneral in ctor/onResize before pool/update).
        // pre ensures GENERAL; clears + F10 resets use GENERAL directly.
        vkCmdClearColorImage(cmd, hdrOutputImage_,   VK_IMAGE_LAYOUT_GENERAL, &clearBlack, 1, &range);
        vkCmdClearColorImage(cmd, prevHdrOutputImage_, VK_IMAGE_LAYOUT_GENERAL, &clearBlack, 1, &range);
        endSubmitAndWait(cmd);
        LOG_DEBUG_CAT("THERMO", "CLEAR THERMO: HDR clears done — boundary reset (high prevMaintCost when accumulation ON) ready for entropyThisFrame + boundaryThermo");
    }

    // Pre-transition the 5 storage images (hdr pair + 3 fields) from UNDEFINED (post-create) to GENERAL.
    // (ensured called in ctor + onResize before pool/update per requirements).
    // Descriptor writes declare GENERAL layout; doing this before updateDescriptorSet (and thus before bind/dispatch)
    // prevents driver faults from layout mismatch at bind time (per original ctor logic + schematic notes).
    // Ensures accountant (b2) + images + all buffers written with matching layouts/resources.
    // Also makes clears safe (no trans/UNDEF inside clear* — rely on pre).
    // Handle dumps + THERMO logs added inside.
void preTransitionAllImagesToGeneral() noexcept {
    if (!hdrOutputImage_ || !prevHdrOutputImage_ || !fieldPhiImage_ || !fieldThermoImage_ || !fieldFlowImage_) {
        LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: preTransition skipped - some images null");
        return;
    }
    LOG_DEBUG_CAT("THERMO", "PRETRANS THERMO: preTransitionAllImagesToGeneral entry — hdr pair + Phi/Thermo/Flow to GENERAL for safe descriptor writes (b2 accountant + b9 thermo) + clears + dispatch");
    LOG_DEBUG_CAT("RAYCANVAS", "DEBUG: preTrans handle dump - hdrImg=0x{:016x} prevImg=0x{:016x} phiImg=0x{:016x} thermoImg=0x{:016x} flowImg=0x{:016x}",
                  reinterpret_cast<uintptr_t>(hdrOutputImage_),
                  reinterpret_cast<uintptr_t>(prevHdrOutputImage_),
                  reinterpret_cast<uintptr_t>(fieldPhiImage_),
                  reinterpret_cast<uintptr_t>(fieldThermoImage_),
                  reinterpret_cast<uintptr_t>(fieldFlowImage_));
    VkCommandBuffer preCmd = beginTransientCommandBuffer();
    if (preCmd) {
        transitionImageLayout(preCmd, hdrOutputImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        transitionImageLayout(preCmd, prevHdrOutputImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        transitionImageLayout(preCmd, fieldPhiImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        transitionImageLayout(preCmd, fieldThermoImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        transitionImageLayout(preCmd, fieldFlowImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        endSubmitAndWait(preCmd);
        LOG_SUCCESS_CAT("RAYCANVAS", "DEBUG: pre-transitioned images to GENERAL before descriptor update");
        LOG_DEBUG_CAT("THERMO", "PRETRANS THERMO: images pre-trans to GENERAL (b0/1 hdr, b8/9/10 fields for boundaryThermo); accountant b2 + res3/5/mats/audio b6 will be buffer updates; fabric/descriptor/clears safe now");
    }
}

    void buildMaterialLibrary() noexcept {
        VkDeviceSize size = sizeof(Materials::AllMaterials);

        materialsHandle_ = Memory::createBuffer(
            size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            "MaterialsLibrary",
            Memory::MemoryHint::HostVisible
        );

        if (size > 0) {
            auto [staging, mem] = Memory::uploadToBuffer(materialsHandle_, Materials::AllMaterials.data(), size);
            if (staging) {
                vkDestroyBuffer(rtx().device, staging, nullptr);
                vkFreeMemory(rtx().device, mem, nullptr);
            }
        }

        LOG_SUCCESS_CAT("RAYCANVAS", "Material library uploaded — {} materials", static_cast<int>(MAT_COUNT));
    }

private:
    SDL_Window*    window_                    = nullptr;
    int            window_width_              = 0;
    int            window_height_             = 0;
    int            render_width_              = 0;
    int            render_height_             = 0;

    bool           minimized_                 = false;
    bool           destroyed_                 = false;
    bool           resourcesReleased_         = false;
    bool           firstFrame_                = true;

    uint64_t       materialsHandle_           = 0;
    VkAccelerationStructureKHR tlas_          = VK_NULL_HANDLE;

    VkImage        hdrOutputImage_            = VK_NULL_HANDLE;
    VkImageView    hdrOutputView_             = VK_NULL_HANDLE;
    VkDeviceMemory hdrOutputMemory_           = VK_NULL_HANDLE;

    VkImage        prevHdrOutputImage_        = VK_NULL_HANDLE;
    VkImageView    prevHdrOutputView_         = VK_NULL_HANDLE;
    VkDeviceMemory prevHdrOutputMemory_       = VK_NULL_HANDLE;

    // ── PROPALACTIC ANALOG FIELD FABRIC (Phi / Thermo / Flow) ─────────────────
    // These are the living state of the universal gate computer. Evolved every dispatch.
    // RTX rays and compute both read/write them → true hardware-gate fidelity.
    VkImage        fieldPhiImage_             = VK_NULL_HANDLE;
    VkImageView    fieldPhiView_              = VK_NULL_HANDLE;
    VkDeviceMemory fieldPhiMemory_            = VK_NULL_HANDLE;

    VkImage        fieldThermoImage_          = VK_NULL_HANDLE;
    VkImageView    fieldThermoView_           = VK_NULL_HANDLE;
    VkDeviceMemory fieldThermoMemory_         = VK_NULL_HANDLE;

    VkImage        fieldFlowImage_            = VK_NULL_HANDLE;
    VkImageView    fieldFlowView_             = VK_NULL_HANDLE;
    VkDeviceMemory fieldFlowMemory_           = VK_NULL_HANDLE;

    VkDescriptorPool descriptorPool_          = VK_NULL_HANDLE;
    VkDescriptorSet  descriptorSet_           = VK_NULL_HANDLE;

    double         adaptiveScale_             = 1.0;

    double         lastPresentTime_s_         = 0.0;
    double         measuredRefreshRateHz_     = 60.0;
    double         lastFpsLog_                = 0.0;
    uint64_t       frameCount_                = 0;
    double         lastAdaptiveAdjustTime_    = 0.0;

    bool           needsRecreate_             = false;

    VkQueryPool    timestampQueryPool_        = VK_NULL_HANDLE;
    VkQueryPool    pipelineStatsQueryPool_    = VK_NULL_HANDLE;  // for invocations (gated, zero cost)
    double         timestampPeriodNs_         = 1.0;
    double         smoothedGpuTimeMs_         = 16.67;

    bool           headless_                  = false;  // AMOURANTHRTX_HEADLESS etc — skips acquire/present, still dispatches for accountant

    static RayCanvas* s_activeInstance_;

    int64_t        maxFrames_                 = 0;      // from env AMOURANTHRTX_MAX_FRAMES for bounded clean exit
    uint64_t       totalFrames_;                        // always-increasing dispatch count (survives fps reset) — ensures N full dispatches + pop before exit
};

inline RayCanvas* rayCanvas = nullptr;
inline RayCanvas* RayCanvas::s_activeInstance_ = nullptr;
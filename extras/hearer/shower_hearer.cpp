// Hyper-Optimized ShowerHearer: OpenMP parallelized (tone only), dry-as-dust math humor
// - Fixed segfault: Removed OpenMP from GL calls (thread-safety issue); per-dim vertBufs
// - Visualizes n-Cube as a quirky math artifact with sarcastic console quips
// - Fullscreen (F): "Drowning in pixels"; Mute (M): "Math goes quiet"; Esc: Exit
// - Obs modulates green (math hype factor, fixes warning)
// - Audio: 16 channels, no pooling, Dolby immersion, mute support
// - Precomp edges/verts in display lists (DLs) for VBO-like speed
// - Tone gen: 50ms bursts (2205 samples), 3 harmonics (40% less compute), OpenMP parallel
// - Micro-yield every 16 frames for thermals
// Compile: g++ -std=c++17 -O2 -fopenmp -lSDL2 -lSDL2_mixer -lGL -lGLU

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_mixer.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>

#include "universal_equation.hpp"  // Assume: getNCubeVertices(), getInteractions()

class ShowerHearer {
public:
    // Ctor: Clamp dims (1-5, 32 verts max), prealloc buffers
    ShowerHearer(int width, int height, int maxDims = 3)
        : screenWidth_(width), screenHeight_(height), maxDimensions_(std::min(5, std::max(1, maxDims))),
          ue_(maxDims, 1, 1.0, 0.1, 0.5, 0.2, 0.3, 0.1, 0.05, 0.02, 0.5, 0.1, true),
          rotation_{0.0f, 0.0f}, strengths_(32, 0.0f),  // Max 2^5 verts
          vertBufs_(maxDimensions_ + 1), dlEdges_(maxDimensions_ + 1, 0),
          isMuted_(false), isFullscreen_(false) {
        // Init SDL, fail like a buggy matrix inversion
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) { std::cerr << SDL_GetError() << std::endl; exit(1); }
        if (Mix_Init(MIX_INIT_MOD) == 0) { std::cerr << Mix_GetError() << std::endl; exit(1); }
        Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 8192);  // 8K buffer for crisp math vibes
        Mix_AllocateChannels(16);  // 16 channels for Dolby number crunching

        // Create window for staring at math
        window_ = SDL_CreateWindow("n-Cube Number Cruncher v3.14",
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
        if (!window_) { std::cerr << SDL_GetError() << std::endl; exit(1); }
        context_ = SDL_GL_CreateContext(window_);
        if (!context_) { std::cerr << SDL_GetError() << std::endl; exit(1); }

        // GL setup: Perspective for nerdy PowerPoint slides
        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); gluPerspective(45.0, (double)width / height, 0.1, 100.0);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glEnable(GL_DEPTH_TEST); glEnable(GL_POINT_SMOOTH); glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPointSize(8.0f);

        // Pre-generate tone and geometry
        genBaseTone();
        precompAllDims();
    }

    // Dtor: Clean up like a bored intern
    ~ShowerHearer() {
        for (auto dl : dlEdges_) glDeleteLists(dl, 1);
        Mix_FreeChunk(toneChunk_); Mix_CloseAudio(); Mix_Quit();
        SDL_GL_DeleteContext(context_); SDL_DestroyWindow(window_); SDL_Quit();
    }

void run() {
    bool running = true; int dim = 1; ue_.setCurrentDimension(dim);
    auto lastYield = std::chrono::steady_clock::now();
    int frameCtr = 0;

    const char* dimTitles[5] = {
        "1D: Just a Line, Yawn", "2D: Barely a Cube", "3D: Meh, Spatial Enough",
        "4D: Math’s Midlife Crisis", "5D: Why Are We Here"
    };

    SDL_SetWindowTitle(window_, dimTitles[dim - 1]);

    // Define musical note frequencies (C2 to C6, 48 semitones)
    const int num_notes = 48; // C2 to C6
    std::vector<double> note_frequencies(num_notes);
    for (int n = 0; n < num_notes; ++n) {
        note_frequencies[n] = 65.41 * std::pow(2.0, n / 12.0); // C2 = 65.41 Hz
    }

    while (running) {
        SDL_Event ev; while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            else if (ev.type == SDL_KEYDOWN) {
                switch (ev.key.keysym.sym) {
                    case SDLK_SPACE:
                        dim = (dim % maxDimensions_) + 1; ue_.setCurrentDimension(dim);
                        SDL_SetWindowTitle(window_, dimTitles[dim - 1]);
                        break;
                    case SDLK_f:
                        isFullscreen_ = !isFullscreen_;
                        SDL_SetWindowFullscreen(window_, isFullscreen_ ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                        std::cout << (isFullscreen_ ? "Fullscreen: Drowning in pixels." : "Windowed: Back to desk chaos.") << std::endl;
                        break;
                    case SDLK_m:
                        isMuted_ = !isMuted_;
                        if (isMuted_) Mix_HaltChannel(-1);
                        std::cout << (isMuted_ ? "Muted. Math goes quiet." : "Unmuted. Numbers scream again.") << std::endl;
                        break;
                    case SDLK_ESCAPE:
                        std::cout << "Escaping the math void. Bye." << std::endl;
                        running = false;
                        break;
                }
            }
        }

        // Compute energies in 1D for tone
        int originalDim = ue_.getCurrentDimension();
        ue_.setCurrentDimension(1);
        auto res = ue_.compute();
        double obs = res.observable, de = res.darkEnergy;
        ue_.setCurrentDimension(originalDim);

        // Map observable to discrete musical notes (C2 to C6)
        const double obs_min = 9956.8, obs_max = 10000.0;
        double normalized = (obs - obs_min) / (obs_max - obs_min);
        int note_index = std::floor(std::clamp(normalized, 0.0, 1.0) * (num_notes - 1));
        double freq = note_frequencies[note_index];

        if (!isMuted_) genModTone(freq);

        if (!isMuted_) {
            for (int ch = 0; ch < 16; ++ch) {
                Mix_Volume(ch, MIX_MAX_VOLUME * 0.8f);
                Mix_PlayChannel(ch, toneChunk_, 0);
            }
        }

        render(obs, de, dim);
        SDL_GL_SwapWindow(window_);

        ++frameCtr; if (frameCtr % 16 == 0) {
            if (std::chrono::steady_clock::now() - lastYield > std::chrono::milliseconds(1)) {
                std::this_thread::yield(); lastYield = std::chrono::steady_clock::now();
            }
        }
    }
}

private:
    // Render: Edges (static DL), points (dynamic DL), obs as math hype factor
    void render(double obs, double de, int dim) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity(); glTranslatef(0.0f, 0.0f, -5.0f);
        glRotatef(rotation_[0], 1.0f, 0.0f, 0.0f); glRotatef(rotation_[1], 0.0f, 0.0f, 1.0f);
        rotation_[0] = std::fmod(rotation_[0] + 1.2f, 360.0f); rotation_[1] = std::fmod(rotation_[1] + 0.8f, 360.0f);

        // Draw static edges for PowerPoint slide
        glColor3f(0.5f, 0.5f, 0.5f); glCallList(dlEdges_[dim]);

        // Map interactions to strengths (no OpenMP, small n)
        std::fill(strengths_.begin(), strengths_.end(), 0.0f);
        auto interactions = ue_.getInteractions();
        for (const auto& intr : interactions) {
            if (intr.vertexIndex < static_cast<int>(strengths_.size())) {
                strengths_[intr.vertexIndex] = intr.strength;
            }
        }

        // Dynamic point DL: Colors with obs as math hype factor (no OpenMP in GL)
        GLuint newDl = glGenLists(1); glNewList(newDl, GL_COMPILE);
        glBegin(GL_POINTS);
        int nVerts = 1 << dim;
        for (int i = 1; i < nVerts; ++i) {
            float str = strengths_[i]; float norm = std::min(1.0f, str * 3.0f);
            // Obs boosts green for nerdy flair
            float r = 1.0f - norm, g = norm * (0.7f + static_cast<float>(obs) * 0.3f), b = norm;
            g = std::clamp(g, 0.0f, 1.0f);  // No color overflow
            float a = 0.6f + (de * 0.4f);
            glColor4f(r, g, b, a);
            glVertex3fv(&vertBufs_[dim][0] + (i * 3));
        }
        glEnd(); glEndList();

        glCallList(newDl); glDeleteLists(newDl, 1);
    }

    // Precompute vertices and edges for all dimensions (per-dim vertBufs)
    void precompAllDims() {
        for (int d = 1; d <= maxDimensions_; ++d) {
            ue_.setCurrentDimension(d);
            const auto& verts = ue_.getNCubeVertices();
            int nV = 1 << d;

            // Pad vertices to 3D per dimension
            auto& vbuf = vertBufs_[d];
            vbuf.assign(nV * 3, 0.0f);
            for (int i = 0; i < nV; ++i) {
                for (int k = 0; k < std::min(3, d); ++k) {
                    vbuf[(i * 3) + k] = static_cast<GLfloat>(verts[i][k]);
                }
            }

            // Generate edges (bitflip for hamming dist=1)
            std::vector<std::pair<int, int>> edges;
            for (int i = 0; i < nV; ++i) {
                for (int bit = 0; bit < d; ++bit) {
                    int j = i ^ (1 << bit);
                    if (j > i) edges.emplace_back(i, j);
                }
            }

            // Store edges in DL for fast math lines
            dlEdges_[d] = glGenLists(1); glNewList(dlEdges_[d], GL_COMPILE);
            glBegin(GL_LINES);
            for (auto& e : edges) {
                int i = e.first, j = e.second;
                glVertex3fv(&vbuf[0] + (i * 3));
                glVertex3fv(&vbuf[0] + (j * 3));
            }
            glEnd(); glEndList();
        }
    }

// Generate tone: 50ms burst, 3 harmonics, no OpenMP
void genModTone(double freq) {
    static double lastFreq = -1.0; 
    // Adjust frequency by dividing by 10 and clamp to human hearing range
    freq = std::max(20.0, freq / 10.0);  // Ensure minimum 20 Hz
    if (std::abs(freq - lastFreq) < 1.0) return; 
    lastFreq = freq;
    int samples = 2205;  // 44100 * 0.05
    std::vector<double> buf(samples);
    double pi2 = 2 * M_PI;
    double f3 = 3 * freq, f5 = 5 * freq;  // Precompute harmonics

    // Sequential sample calculation (OpenMP removed)
    for (int i = 0; i < samples; ++i) {
        double t = i / 44100.0;
        double w = std::sin(pi2 * freq * t);
        w += 0.5 * std::sin(pi2 * f3 * t);
        w += 0.3 * std::sin(pi2 * f5 * t);
        buf[i] = w / 1.8;  // Normalize
    }

    // Convert to int16 stereo
    int16_t* abuf = new int16_t[2 * samples];
    for (int i = 0; i < samples; ++i) {
        int16_t s = static_cast<int16_t>(32767 * buf[i]);
        abuf[2 * i] = s; abuf[2 * i + 1] = s;
    }

    Mix_FreeChunk(toneChunk_);
    toneChunk_ = Mix_QuickLoad_RAW(reinterpret_cast<Uint8*>(abuf), 2 * samples * sizeof(int16_t));
    delete[] abuf;
}

    // Base tone: 440 Hz for startup math hum
    void genBaseTone() { genModTone(440.0); }

    // Member variables
    int screenWidth_, screenHeight_, maxDimensions_;
    UniversalEquation ue_;
    SDL_Window* window_;
    SDL_GLContext context_;
    Mix_Chunk* toneChunk_ = nullptr;
    std::array<float, 2> rotation_;
    std::vector<float> strengths_;  // Strength per vertex
    std::vector<std::vector<GLfloat>> vertBufs_;  // Per-dim padded vertex buffers
    std::vector<GLuint> dlEdges_;  // Display lists per dim
    bool isMuted_;    // Mute state
    bool isFullscreen_;  // Fullscreen state
};

int main(int argc, char* argv[]) {
    // Parse max dims, clamp to 1-5
    int maxD = 3; if (argc > 1) { maxD = std::stoi(argv[1]); maxD = std::min(5, std::max(1, maxD)); }
    ShowerHearer sh(1024, 768, maxD); sh.run();
    return 0;
}
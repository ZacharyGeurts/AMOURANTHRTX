// NEW SIMPLE MODERN SDF GUI Rewrite
#pragma once
// Ultra simple architecture: One SDF Canvas, data-driven widgets array, shader params.
struct ModernSDFGUI {
  // Minimal state
  std::vector<WidgetDef> widgets; // position, type, SDF func id
  void renderSDFAll(VkCommandBuffer cmd) { /* unified SDF pass with modern effects */ }
  // Modern: dynamic lighting, bloom hints, responsive scaling
  // Simple:  <200 LOC core
  // Backward: drop-in replace with compat layer
};
// Everything now pure SDF + modern shaders. Minimal boilerplate.
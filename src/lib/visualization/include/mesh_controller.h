#pragma once
#include <memory>
#include <Eigen/Core>

#include "camera.h"            // your existing camera
#include "mesh_construction.h" // Model (MeshConstructor / Mesh bundles)
#include "mesh_renderer.h"     // View
#include "optimization.h"      // WeightFunction

namespace spherical_tiling {

class MeshController {
public:
  struct Params {
    double radius = 1.0;
    uint16_t    frequency = 3;              // Goldberg frequency q
    bool   runOptimization = false;
    WeightFunction weightFunc = WeightFunction::F1;
  };

  explicit MeshController(std::shared_ptr<MeshConstructor> model,
                          std::shared_ptr<MeshRenderer>   view);

  // --- Configuration / model control ---
  void setParams(const Params& p);
  const Params& params() const { return params_; }
  void requestRebuild();               // mark dirty; call rebuildIfNeeded() in your frame loop
  void rebuildIfNeeded();              // does nothing if not dirty

  // --- Camera / viewport control ---
  void resetCamera();
  void setPerspective(float fovDeg, float aspect, float nearZ, float farZ);
  void setViewportSize(int fbWidth, int fbHeight) { fbWidth_ = fbWidth; fbHeight_ = fbHeight; }
  // Input hooks (call from your windowing system; supply whether UI wants the mouse)
  void onMouseButton(bool leftDown, double x, double y, bool uiWantsMouse);
  void onCursorPos(double x, double y, bool uiWantsMouse);
  void onScroll(double yoffset, bool uiWantsMouse);

  // --- Per-frame rendering ---
  // Call after your GL context is current and the MeshRenderer has a valid shader.
  void renderFrame();

  // --- Optional: ImGui UI (call between ImGui::NewFrame and ImGui::Render) ---
  // If you don’t use ImGui, you can ignore these and drive params/visibility externally.
  void drawUI();           // creates menu + panels
  void drawHudStats();     // minimal stats-only panel

private:
  void rebuild_();         // immediate rebuild, throws on failure

  std::shared_ptr<MeshConstructor> model_;
  std::shared_ptr<MeshRenderer>    view_;

  Params     params_;
  Visibility vis_;
  bool       needsRebuild_ = true;

  // Camera & interaction
  Camera camera_;
  int fbWidth_ = 1280, fbHeight_ = 720;
  double lastX_ = 0.0, lastY_ = 0.0;
  bool dragging_ = false;
};

} // namespace spherical_tiling

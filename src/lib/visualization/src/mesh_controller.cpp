#include "mesh_controller.h"
#include <iostream>
#include <stdexcept>

#ifdef IMGUI_VERSION
  #include <imgui.h>
#endif

namespace spherical_tiling {

MeshController::MeshController(std::shared_ptr<MeshConstructor> model,
                                         std::shared_ptr<MeshRenderer>   view)
: model_(std::move(model)), view_(std::move(view)) {
  if (!model_ || !view_) throw std::runtime_error("Controller requires model and view");
  // Reasonable camera defaults
  camera_.reset();
  camera_.setPerspective(45.0f, float(fbWidth_)/float(fbHeight_), 0.1f, 100.0f);

  // set default visibility
  vis_.isIcosahedronVisible_    = false;
  vis_.isSubdivisionVisible_    = false;
  vis_.isPrimalMeshVisible_     = true;
  vis_.isPrimalDebugMeshVisible_= false;
  vis_.isDualMeshVisible_       = true;
}

void MeshController::setParams(const Params& p) {
  params_ = p;
  requestRebuild();
}

void MeshController::requestRebuild() { needsRebuild_ = true; }

void MeshController::rebuildIfNeeded() {
  if (!needsRebuild_) return;
  rebuild_();
  needsRebuild_ = false;
}

void MeshController::rebuild_() {
  const double R = params_.radius;
  const uint16_t q = params_.frequency;
  model_.reset(new MeshConstructor(R, uint16_t(q)));
  if (params_.runOptimization) {
    // TODO: implement optimization
    std::cout << "Optimization requested (not yet implemented)" << std::endl;
  }
  view_->setMeshConstruct(model_);
}

void MeshController::resetCamera() { camera_.reset(); }

void MeshController::setPerspective(float fovDeg, float aspect, float nearZ, float farZ) {
  camera_.setPerspective(fovDeg, aspect, nearZ, farZ);
}

void MeshController::onMouseButton(bool leftDown, double x, double y, bool uiWantsMouse) {
  if (uiWantsMouse) { dragging_ = false; return; }
  if (leftDown) {
    dragging_ = true; lastX_ = x; lastY_ = y;
  } else {
    dragging_ = false;
  }
}

void MeshController::onCursorPos(double x, double y, bool uiWantsMouse) {
  if (uiWantsMouse || !dragging_) { lastX_ = x; lastY_ = y; return; }
  double dx = x - lastX_;
  double dy = y - lastY_;
  camera_.rotate(float(dx) * 0.01f, float(-dy) * 0.01f);
  lastX_ = x; lastY_ = y;
}

void MeshController::onScroll(double yoffset, bool uiWantsMouse) {
  if (uiWantsMouse) return;
  camera_.zoom(float(-yoffset) * 0.3f);
}

void MeshController::renderFrame() {
  // Ensure geometry is current
  rebuildIfNeeded();

  // Compute MVP (Eigen → float 4×4)
  Eigen::Matrix4f VP = camera_.getProjectionMatrix() * camera_.getViewMatrix();
  // Convert to glm::mat4
  glm::mat4 glmVP;
  std::memcpy(&glmVP[0][0], VP.data(), 16*sizeof(float));

  view_->renderAllMeshes(glmVP, vis_);
}

void MeshController::drawHudStats() {
#ifdef IMGUI_VERSION
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(260, 150), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Sphere Info")) {
    ImGui::Text("Frequency: %d", params_.frequency);
    ImGui::Text("Radius:    %.3f", params_.radius);
    ImGui::Text("Optimize:  %s", params_.runOptimization ? "Yes" : "No");
    ImGui::Separator();
    ImGui::Text("Camera:");
    ImGui::Text("  distance: %.2f", camera_.getDistance());
    ImGui::Text("  yaw:      %.1f deg",  camera_.getYaw()   * 180.0f/float(M_PI));
    ImGui::Text("  pitch:    %.1f deg",  camera_.getPitch() * 180.0f/float(M_PI));
  }
  ImGui::End();
#endif
}

void MeshController::drawUI() {
#ifdef IMGUI_VERSION
  // Menu
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("Sphere")) {
      if (ImGui::MenuItem("Reset Camera")) { resetCamera(); }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Build")) {
      static int q = params_.frequency;
      static bool runOpt = params_.runOptimization;
      static int wfIdx = int(params_.weightFunc);

      ImGui::InputInt("Frequency q", &q);
      if (q < 1) q = 1; if (q > 50) q = 50;
      ImGui::Checkbox("Run optimization", &runOpt);

      const char* wnames[] = { "f1", "f2", "f3", "f4", "f5 (area)", "f6 (angle)" };
      ImGui::Combo("Weight", &wfIdx, wnames, 6);

      if (ImGui::MenuItem("Rebuild")) {
        Params p = params_;
        p.frequency = q;
        p.runOptimization = runOpt;
        p.weightFunc = static_cast<WeightFunction>(wfIdx);
        setParams(p);
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      ImGui::Checkbox("Icosahedron",    &vis_.isIcosahedronVisible_);
      ImGui::Checkbox("Subdivided",     &vis_.isSubdivisionVisible_);
      ImGui::Separator();
      ImGui::Checkbox("Primal (normal)",&vis_.isPrimalMeshVisible_);
      ImGui::Checkbox("Primal (debug)", &vis_.isPrimalDebugMeshVisible_);
      ImGui::Separator();
      ImGui::Checkbox("Dual",           &vis_.isDualMeshVisible_);
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

  drawHudStats();
#endif
}

} // namespace spherical_tiling

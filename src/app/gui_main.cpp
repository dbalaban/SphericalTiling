#include "camera.h"
#include "mesh_construction.h"
#include "mesh_renderer.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>

using namespace spherical_tiling;

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr const char* kGlslVersion = "#version 330";

struct AppState {
  double radius = 1.0;
  int frequency = 3;

  bool showEarth = true;
  bool showIcosahedron = false;
  bool showSubdivision = false;
  bool showPrimal = true;
  bool showPrimalDebug = false;
  bool showDual = true;
  bool enableMorph = true;

  bool needsRebuild = true;
  bool dragging = false;
  double lastMouseX = 0.0;
  double lastMouseY = 0.0;
  int framebufferWidth = kWindowWidth;
  int framebufferHeight = kWindowHeight;

  Camera camera;
  std::shared_ptr<MeshRenderer> renderer;
  std::shared_ptr<MeshConstructor> mesh;
  std::string earthStatus;
};

void glfwErrorCallback(int error, const char* description) {
  std::cerr << "[GLFW Error " << error << "]: " << description << '\n';
}

Visibility getVisibility(const AppState& state) {
  return Visibility{
    state.showEarth,
    state.showIcosahedron,
    state.showSubdivision,
    state.showPrimal,
    state.showPrimalDebug,
    state.showDual,
  };
}

void rebuildMesh(AppState& state) {
  state.mesh = std::make_shared<MeshConstructor>(state.radius, static_cast<uint16_t>(state.frequency));
  state.renderer->setMeshConstruct(state.mesh);
  state.needsRebuild = false;
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
  auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
  if (!state) {
    return;
  }

  state->framebufferWidth = width;
  state->framebufferHeight = height;
  glViewport(0, 0, width, height);
  if (height > 0) {
    state->camera.setPerspective(45.0f, static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);
  }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  (void)mods;
  auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
  if (!state || button != GLFW_MOUSE_BUTTON_LEFT) {
    return;
  }

  ImGuiIO& io = ImGui::GetIO();
  if (action == GLFW_PRESS && !io.WantCaptureMouse) {
    state->dragging = true;
    glfwGetCursorPos(window, &state->lastMouseX, &state->lastMouseY);
  } else if (action == GLFW_RELEASE) {
    state->dragging = false;
  }
}

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
  auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
  if (!state) {
    return;
  }

  if (!state->dragging) {
    state->lastMouseX = xpos;
    state->lastMouseY = ypos;
    return;
  }

  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse) {
    state->dragging = false;
    return;
  }

  const double dx = xpos - state->lastMouseX;
  const double dy = ypos - state->lastMouseY;
  // Horizontal drag changes azimuth; vertical drag changes latitude.
  state->camera.rotate(static_cast<float>(-dx) * 0.01f, static_cast<float>(dy) * 0.01f);
  state->lastMouseX = xpos;
  state->lastMouseY = ypos;
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
  (void)window;
  (void)xoffset;
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse) {
    return;
  }

  auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
  if (state) {
    state->camera.zoom(static_cast<float>(-yoffset) * 0.3f);
  }
}

float computeMorphFactor(const AppState& state) {
  if (!state.enableMorph) {
    return 0.0f;
  }

  const float zoomStart = 3.6f;
  const float zoomEnd = 1.2f;
  const float distance = state.camera.getDistance();
  float factor = (zoomStart - distance) / (zoomStart - zoomEnd);
  if (factor < 0.0f) factor = 0.0f;
  if (factor > 1.0f) factor = 1.0f;
  return factor * factor * (3.0f - 2.0f * factor);
}

void renderUi(AppState& state) {
  static const double minRadius = 0.25;
  static const double maxRadius = 5.0;

  ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(330, 380), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Spherical Tiling")) {
    ImGui::Text("Earth tile morph viewer");
    ImGui::Separator();

    bool rebuildRequested = false;
    rebuildRequested |= ImGui::SliderInt("Frequency (q)", &state.frequency, 1, 24);
    rebuildRequested |= ImGui::SliderScalar("Radius", ImGuiDataType_Double, &state.radius, &minRadius, &maxRadius, "%.2f");

    if (ImGui::Button("Rebuild")) {
      state.needsRebuild = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Camera")) {
      state.camera.reset();
    }

    if (rebuildRequested) {
      ImGui::TextUnformatted("Press Rebuild to apply mesh changes");
    }

    ImGui::SeparatorText("Overlays");
    ImGui::Checkbox("Earth Tiles", &state.showEarth);
    ImGui::Checkbox("Icosahedron", &state.showIcosahedron);
    ImGui::Checkbox("Subdivision", &state.showSubdivision);
    ImGui::Checkbox("Primal", &state.showPrimal);
    ImGui::Checkbox("Primal Debug", &state.showPrimalDebug);
    ImGui::Checkbox("Dual Outline", &state.showDual);

    ImGui::SeparatorText("Morph");
    ImGui::Checkbox("Enable Zoom Morph", &state.enableMorph);
    ImGui::Text("Morph factor: %.2f", computeMorphFactor(state));
    ImGui::Text("Camera distance: %.2f", state.camera.getDistance());
    ImGui::Text("Azimuth: %.1f deg", state.camera.getAzimuth() * 180.0f / static_cast<float>(M_PI));
    ImGui::Text("Latitude: %.1f deg", state.camera.getLatitude() * 180.0f / static_cast<float>(M_PI));

    if (state.mesh) {
      const Mesh& subdivided = state.mesh->getSubdividedMesh();
      const Mesh& dual = state.mesh->getDualMesh();
      const DualTopologyReport& report = state.renderer->getValidationReport();
      ImGui::SeparatorText("Counts");
      ImGui::Text("Subdivided vertices: %ld", static_cast<long>(subdivided.vertices.cols()));
      ImGui::Text("Subdivided faces:    %ld", static_cast<long>(subdivided.faces.size()));
      ImGui::Text("Dual faces:          %ld", static_cast<long>(dual.faces.size()));
      ImGui::Text("Pentagons:           %ld", static_cast<long>(report.pentagons));
      ImGui::Text("Hexagons:            %ld", static_cast<long>(report.hexagons));
      ImGui::Text("Topology:            %s", report.valid ? "valid" : "invalid");
      if (!report.valid && !report.errors.empty()) {
        ImGui::TextWrapped("%s", report.errors.front().c_str());
      }
    }

    ImGui::SeparatorText("Earth Raster");
    ImGui::TextWrapped("%s", state.earthStatus.c_str());
  }
  ImGui::End();
}

} // namespace

int main() {
  glfwSetErrorCallback(glfwErrorCallback);
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW\n";
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Spherical Tiling Viewer", nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window\n";
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  if (!gladLoadGL(glfwGetProcAddress)) {
    std::cerr << "Failed to initialize GLAD\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  AppState state;
  state.renderer = std::make_shared<MeshRenderer>();
  state.camera.setPerspective(45.0f, static_cast<float>(kWindowWidth) / static_cast<float>(kWindowHeight), 0.1f, 100.0f);
  std::string textureError;
  if (state.renderer->loadEarthTexture("assets/earth/blue_marble_5400x2700_december.jpg", textureError)) {
    state.earthStatus = "Loaded assets/earth/blue_marble_5400x2700_december.jpg";
  } else {
    state.earthStatus = "Earth raster load failed: " + textureError;
  }

  glfwSetWindowUserPointer(window, &state);
  glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
  glfwSetMouseButtonCallback(window, mouseButtonCallback);
  glfwSetCursorPosCallback(window, cursorPosCallback);
  glfwSetScrollCallback(window, scrollCallback);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(kGlslVersion);

  glEnable(GL_DEPTH_TEST);
  glClearColor(0.92f, 0.94f, 0.98f, 1.0f);

  try {
    rebuildMesh(state);
  } catch (const std::exception& ex) {
    std::cerr << "Initial mesh build failed: " << ex.what() << '\n';
  }

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    if (state.needsRebuild) {
      try {
        rebuildMesh(state);
      } catch (const std::exception& ex) {
        std::cerr << "Mesh rebuild failed: " << ex.what() << '\n';
        state.needsRebuild = false;
      }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    renderUi(state);
    ImGui::Render();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Eigen::Matrix4f vp = state.camera.getProjectionMatrix() * state.camera.getViewMatrix();
    glm::mat4 glmVp;
    std::memcpy(&glmVp[0][0], vp.data(), 16 * sizeof(float));
    state.renderer->renderAllMeshes(glmVp, getVisibility(state), state.camera.getEyePosition(), computeMorphFactor(state));

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

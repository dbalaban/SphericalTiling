#include "camera.h"
#include "mesh_renderer.h"
#include "goldberg_subdivision.h"
#include "tile_graph.h"
#include "dual_construction.h"
#include "optimization.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <exception>
#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

#ifdef __GNUC__
#include <execinfo.h>
#include <cxxabi.h>
#include <dlfcn.h>
#endif

using namespace spherical_tiling;

// Stack trace utility for debugging
void printStackTrace() {
#ifdef __GNUC__
    std::cerr << "\n=== Stack Trace ===" << std::endl;
    std::cerr.flush();
    
    void* array[50];
    int size = backtrace(array, 50);
    char** messages = backtrace_symbols(array, size);
    
    if (messages == nullptr) {
        std::cerr << "Failed to get backtrace symbols" << std::endl;
        std::cerr.flush();
        return;
    }
    
    for (int i = 0; i < size; ++i) {
        Dl_info info;
        if (dladdr(array[i], &info) && info.dli_sname) {
            char* demangled = nullptr;
            int status = -1;
            demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
            std::cerr << "  [" << i << "] " 
                      << (status == 0 ? demangled : info.dli_sname) 
                      << " + " << (void*)((char*)array[i] - (char*)info.dli_saddr)
                      << std::endl;
            free(demangled);
        } else {
            std::cerr << "  [" << i << "] " << messages[i] << std::endl;
        }
        std::cerr.flush();
    }
    std::cerr << "===================" << std::endl;
    std::cerr.flush();
    free(messages);
#else
    std::cerr << "Stack trace not available on this platform" << std::endl;
    std::cerr.flush();
#endif
}

// Signal handler for segmentation faults
void signalHandler(int signal) {
    // Flush all output buffers immediately
    std::cerr.flush();
    std::cout.flush();
    
    std::cerr << "\n!!! SEGMENTATION FAULT DETECTED !!!" << std::endl;
    std::cerr << "Signal: " << signal << " (SIGSEGV)" << std::endl;
    std::cerr << "This indicates a memory access violation." << std::endl;
    
    printStackTrace();
    
    // Flush again before exiting
    std::cerr.flush();
    
    // Use _exit() instead of exit() to bypass cleanup that might cause issues
    _exit(signal);
}

// Application state
struct AppState {
    // Sphere parameters
    double radius = 1.0;
    int frequency = 3;
    WeightFunction weightFunc = WeightFunction::F1;
    bool runOptimization = true;
    
    // Mesh data
    std::vector<Eigen::Vector3d> primalVertices;
    std::vector<Eigen::Vector3i> primalFaces;
    std::unique_ptr<TileGraph> graph;
    
    // Display options
    bool showPrimal = true;
    bool showDual = true;
    bool showTriangles = false;
    
    // UI state
    bool showCreateDialog = false;
    bool needsRebuild = true;
};

// Camera and rendering
Camera camera;
MeshRenderer* renderer = nullptr; // Delayed initialization after OpenGL setup
AppState appState;

// Mouse state for camera control
double lastMouseX = 0.0;
double lastMouseY = 0.0;
bool mousePressed = false;

void buildSphere() {
    try {
        // Generate icosahedron
        appState.primalVertices = generateIcosahedron(appState.radius);
        auto icoFaces = generateIcosahedronFaces();
        
        // Perform Goldberg subdivision
        auto subdivision = goldbergSubdivision(appState.primalVertices, icoFaces, 
                                               appState.frequency, appState.radius);
        appState.primalVertices = subdivision.vertices;
        appState.primalFaces = subdivision.faces;
        
        // Build tile graph
        appState.graph = std::make_unique<TileGraph>();
        appState.graph->buildFromTriangulation(subdivision.vertices, subdivision.faces, 
                                               appState.radius);
        
        // Construct dual cells
        constructDualCells(*appState.graph, appState.radius);
        
        // Optimize if requested
        if (appState.runOptimization) {
            int maxIter = (appState.frequency <= 2) ? 50 : 100;
            optimizeTileGraph(*appState.graph, appState.radius, appState.weightFunc, maxIter, true);
            constructDualCells(*appState.graph, appState.radius);
        }
        
        // Update renderer
        // Primal mesh: adjacency graph of dual cells (TileGraph edges connecting pentagon/hexagon centers)
        renderer->setPrimalMesh(*appState.graph, appState.radius);
        // Dual mesh: boundaries of dual cells (pentagon/hexagon edges formed by circumcenters)
        renderer->setDualMesh(*appState.graph, appState.radius);
        // Triangle mesh: edges of the triangular subdivision
        renderer->setTriangleMesh(appState.primalVertices, appState.primalFaces);
        
        appState.needsRebuild = false;
        
        std::cout << "Built sphere: frequency=" << appState.frequency 
                  << ", vertices=" << appState.primalVertices.size()
                  << ", faces=" << appState.primalFaces.size() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "ERROR in buildSphere(): " << e.what() << std::endl;
        printStackTrace();
        appState.needsRebuild = false; // Prevent infinite rebuild loop
    } catch (...) {
        std::cerr << "UNKNOWN ERROR in buildSphere()" << std::endl;
        printStackTrace();
        appState.needsRebuild = false; // Prevent infinite rebuild loop
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods; // Unused parameter
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            // Check if ImGui wants to capture the mouse
            ImGuiIO& io = ImGui::GetIO();
            if (!io.WantCaptureMouse) {
                mousePressed = true;
                glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
            }
        } else if (action == GLFW_RELEASE) {
            mousePressed = false;
        }
    }
}

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    (void)window; // Unused parameter
    if (mousePressed) {
        double deltaX = xpos - lastMouseX;
        double deltaY = ypos - lastMouseY;
        
        camera.rotate(deltaX * 0.01f, -deltaY * 0.01f);
        
        lastMouseX = xpos;
        lastMouseY = ypos;
    }
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;  // Unused parameter
    (void)xoffset; // Unused parameter
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        camera.zoom(-yoffset * 0.3f);
    }
}

void renderUI() {
    // Main menu bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Sphere")) {
            if (ImGui::MenuItem("Create New Sphere")) {
                appState.showCreateDialog = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Camera")) {
                camera.reset();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    
    // Display options panel
    ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 150), ImGuiCond_FirstUseEver);
    ImGui::Begin("Display Options");
    
    ImGui::Text("Mesh Visibility:");
    if (ImGui::Checkbox("Show Primal (Red)", &appState.showPrimal)) {}
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Adjacency graph of dual cells\n(connects centers of pentagons/hexagons)");
    }
    if (ImGui::Checkbox("Show Dual (Black)", &appState.showDual)) {}
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Boundaries of dual cells\n(pentagon/hexagon edges)");
    }
    if (ImGui::Checkbox("Show Triangles (Green)", &appState.showTriangles)) {}
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Edges of triangular subdivision");
    }
    
    ImGui::Separator();
    ImGui::Text("Camera:");
    ImGui::Text("  Distance: %.2f", camera.getDistance());
    ImGui::Text("  Yaw: %.1f°", camera.getYaw() * 180.0f / M_PI);
    ImGui::Text("  Pitch: %.1f°", camera.getPitch() * 180.0f / M_PI);
    
    if (ImGui::Button("Reset Camera")) {
        camera.reset();
    }
    
    ImGui::End();
    
    // Sphere info panel
    ImGui::SetNextWindowPos(ImVec2(10, 190), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 150), ImGuiCond_FirstUseEver);
    ImGui::Begin("Sphere Info");
    
    ImGui::Text("Parameters:");
    ImGui::Text("  Frequency: %d", appState.frequency);
    ImGui::Text("  Radius: %.2f", appState.radius);
    ImGui::Text("  Optimization: %s", appState.runOptimization ? "Yes" : "No");
    
    if (appState.graph) {
        ImGui::Separator();
        ImGui::Text("Statistics:");
        ImGui::Text("  Primal vertices: %zu", appState.primalVertices.size());
        ImGui::Text("  Primal faces: %zu", appState.primalFaces.size());
        ImGui::Text("  Dual nodes: %zu", appState.graph->numNodes());
    }
    
    ImGui::End();
    
    // Create sphere dialog
    if (appState.showCreateDialog) {
        ImGui::SetNextWindowPos(ImVec2(400, 200), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        ImGui::Begin("Create New Sphere", &appState.showCreateDialog);
        
        static int newFrequency = 3;
        static int weightFuncIndex = 0;
        static bool newRunOpt = true;
        
        ImGui::Text("Sphere Parameters:");
        ImGui::Separator();
        
        ImGui::InputInt("Q-Frequency", &newFrequency);
        // Clamp to reasonable range
        if (newFrequency < 1) newFrequency = 1;
        if (newFrequency > 20) newFrequency = 20;
        ImGui::Text("Expected dual cells: %d", 10 * newFrequency * newFrequency + 2);
        
        ImGui::Separator();
        ImGui::Checkbox("Run Optimization", &newRunOpt);
        
        if (newRunOpt) {
            const char* weightFuncs[] = {
                "f1: 2|lat|/π (angles at poles)",
                "f2: 1-f1 (area at poles)",
                "f3: cos²(lat) (area at poles)",
                "f4: sin²(lat) (angles at poles)",
                "f5: 1 (pure area)",
                "f6: 0 (pure angle)"
            };
            ImGui::Combo("Weight Function", &weightFuncIndex, weightFuncs, 6);
        }
        
        ImGui::Separator();
        
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            appState.frequency = newFrequency;
            appState.weightFunc = static_cast<WeightFunction>(weightFuncIndex);
            appState.runOptimization = newRunOpt;
            appState.needsRebuild = true;
            appState.showCreateDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            appState.showCreateDialog = false;
        }
        
        ImGui::End();
    }
}

int main() {
    // Install signal handlers for crashes
    std::signal(SIGSEGV, signalHandler);
    std::signal(SIGABRT, signalHandler);
    
    // Set stderr to unbuffered for immediate crash report output
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    
    std::cerr << "Spherical Tiling GUI starting..." << std::endl;
    std::cerr << "Signal handlers installed (SIGSEGV, SIGABRT)" << std::endl;
    
    try {
        // Initialize GLFW
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return -1;
        }
        
        // Set OpenGL version (3.3 Core)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
        
        // Create window
        GLFWwindow* window = glfwCreateWindow(1280, 720, "Spherical Tiling - GUI Viewer", nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return -1;
        }
        
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // Enable vsync
        
        // Set callbacks
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, cursorPosCallback);
        glfwSetScrollCallback(window, scrollCallback);
        
        // Initialize GLAD
        if (!gladLoadGL(glfwGetProcAddress)) {
            std::cerr << "Failed to initialize GLAD" << std::endl;
            glfwDestroyWindow(window);
            glfwTerminate();
            return -1;
        }
        
        // Setup ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        if (!ImGui::GetCurrentContext()) {
            std::cerr << "Failed to create ImGui context" << std::endl;
            glfwDestroyWindow(window);
            glfwTerminate();
            return -1;
        }
        
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        
        ImGui::StyleColorsDark();
        
        if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
            std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
            ImGui::DestroyContext();
            glfwDestroyWindow(window);
            glfwTerminate();
            return -1;
        }
        
        if (!ImGui_ImplOpenGL3_Init("#version 330")) {
            std::cerr << "Failed to initialize ImGui OpenGL3 backend" << std::endl;
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            glfwDestroyWindow(window);
            glfwTerminate();
            return -1;
        }
        
        // Setup OpenGL state
        glEnable(GL_DEPTH_TEST);
        glLineWidth(2.0f);
        
        // Initialize renderer after OpenGL is ready
        std::cerr << "Initializing MeshRenderer..." << std::endl;
        renderer = new MeshRenderer();
        std::cerr << "MeshRenderer initialized successfully" << std::endl;
        
        // Build initial sphere
        buildSphere();
        
        // Main loop
        while (!glfwWindowShouldClose(window)) {
            try {
                glfwPollEvents();
                
                // Rebuild sphere if needed
                if (appState.needsRebuild) {
                    buildSphere();
                }
                
                // Get framebuffer size
                int displayWidth, displayHeight;
                glfwGetFramebufferSize(window, &displayWidth, &displayHeight);
                
                // Update camera projection
                float aspect = static_cast<float>(displayWidth) / static_cast<float>(displayHeight);
                camera.setPerspective(45.0f, aspect, 0.1f, 100.0f);
                
                // Clear screen
                glViewport(0, 0, displayWidth, displayHeight);
                glClearColor(0.2f, 0.2f, 0.25f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                
                // Render meshes
                Eigen::Matrix4f viewProj = camera.getProjectionMatrix() * camera.getViewMatrix();
                
                if (appState.showPrimal) {
                    renderer->renderPrimal(viewProj, Eigen::Vector3f(1.0f, 0.0f, 0.0f)); // Red
                }
                
                if (appState.showDual) {
                    renderer->renderDual(viewProj, Eigen::Vector3f(0.0f, 0.0f, 0.0f)); // Black
                }
                
                if (appState.showTriangles) {
                    renderer->renderTriangles(viewProj, Eigen::Vector3f(0.0f, 1.0f, 0.0f)); // Green
                }
                
                // Render UI
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();
                
                renderUI();
                
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                
                glfwSwapBuffers(window);
            } catch (const std::exception& e) {
                std::cerr << "ERROR in render loop: " << e.what() << std::endl;
                printStackTrace();
                // Continue running but skip this frame
            } catch (...) {
                std::cerr << "UNKNOWN ERROR in render loop" << std::endl;
                printStackTrace();
                // Continue running but skip this frame
            }
        }
        
        // Cleanup
        if (renderer) {
            delete renderer;
            renderer = nullptr;
        }
        
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow(window);
        glfwTerminate();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR in main(): " << e.what() << std::endl;
        printStackTrace();
        if (renderer) {
            delete renderer;
            renderer = nullptr;
        }
        glfwTerminate();
        return -1;
    } catch (...) {
        std::cerr << "UNKNOWN FATAL ERROR in main()" << std::endl;
        printStackTrace();
        if (renderer) {
            delete renderer;
            renderer = nullptr;
        }
        glfwTerminate();
        return -1;
    }
}

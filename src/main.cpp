#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <queue>
#include <map>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <filesystem>
#include <array>
#include <cctype>
#include <sstream>
#include <iomanip>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <shellapi.h>
#endif

static const double kSpeedUnitsPerHour = 50.0;   // 50 distance-units per hour
static const double kCostPerUnit       = 3.2;    // 3.2 BDT per distance-unit
static const char*  kCurrency          = "BDT";  // label for currency
// Scale for the path-info text only (distance/time/cost). 1.0 = default size.
static const float  kPathInfoTextScale = 0.65f;  // try 0.50f .. 0.85f

// HUD panel styling
static const float  kHudMarginLeftPx   = 12.0f;
static const float  kHudMarginBottomPx = 12.0f;
static const float  kHudPaddingPx      = 8.0f;   // inside the panel
static const float  kHudLineGapBasePx  = 6.0f;   // will be scaled by font scale
static const float  kHudPanelAlpha     = 0.35f;  // 0..1 transparency
static const float  kHudPanelGray      = 0.0f;   // black panel

// ---------------- Shaders ----------------
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
void main(){
    gl_Position = vec4(aPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 nodeColor;
uniform float uAlpha; // NEW: per-draw alpha
void main(){
    FragColor = vec4(nodeColor, uAlpha);
}
)";

// ---------------- Data types (classes) ----------------
class Node {
public:
    float x{}, y{};
    std::string name;

    Node() = default;
    Node(float x_, float y_, std::string name_) : x(x_), y(y_), name(std::move(name_)) {}
};

class WeightedLine {
public:
    int start{}, end{};
    double weight{};

    WeightedLine() = default;
    WeightedLine(int s, int e, double w) : start(s), end(e), weight(w) {}
};

// ---------------- Graph / render state ----------------
std::vector<Node> nodes;
std::vector<int> lines; // pairs of indices
std::vector<WeightedLine> linesWithWeights;
std::vector<std::vector<std::pair<int,double>>> adjacencyList;

int selectedNodeIndex1 = -1;
int selectedNodeIndex2 = -1;
int lastClickedNodeIndex = -1;

std::vector<int> pathIndices;
double totalPathDistance = 0.0;

// GL
unsigned int shaderProgram;
unsigned int VAO_nodes=0, VBO_nodes=0;
unsigned int VAO_lines=0, VBO_lines=0;

int windowW=800, windowH=600;

// ---------------- Images per node ----------------
std::unordered_map<std::string, std::vector<std::string>> nodeImages = {
    {"Rangpur",    {"Tajhat Palace.jpg", "Vinno Jagat.jpg", "Ramsagar.jpg"}},
    {"Sylhet",     {"Tea Garden.jpg", "Jaflong.jpg", "Ratargul.jpg"}},
    {"Khulna",     {"Sundarbans.jpg", "Rupsha River.webp", "Gollamari.webp"}},
    {"Chittagong", {"karnafuli lake.jpg", "Patenga and Naval Academy.jpg", "Chandranath Temple.jpg"}},
    {"Dhaka",      {"Ahsan Monjil.jpg", "Lalbag Kella.jpg", "Parliament House.jpg"}},
    {"Rajshahi",   {"Padma River.webp", "Museum.jpg", "Shah Mokdum Majar.jpg"}},
    {"Barishal",   {"Baitul Aman Mosque.jpg", "Sixty DOmes Mosque.jpg", "Shrine of Khan Zahan Ali.jpg"}}
};

std::unordered_map<int,int> curImageIdxForNode;

// ---------------- Text rendering (5x7 dot font) ----------------
unsigned int VAO_text = 0, VBO_text = 0;

void setupTextBuffers();
void drawAllNodeLabels();
void drawLabelAtNDC(float ndc_x, float ndc_y, const std::string& text);

// Scaled versions used for path info (smaller font)
void drawLabelAtNDCScaled(float ndc_x, float ndc_y, const std::string& text, float scale);
void drawLabelAtNDCWithExtraAboveScaled(float ndc_x, float ndc_y, const std::string& text,
                                        float extraAbovePx, float scale);

// NEW: draw text at exact pixel position (top-left anchor)
void drawLabelAtPixelScaled(float px_left, float py_top, const std::string& text, float scale);

// NEW: helpers to measure text width (px) for panel sizing
float measureTextWidthPx(const std::string& text, float scale);

// NEW: draw a semi-transparent pixel-anchored rectangle (panel)
void drawPixelRect(float px_left, float py_top, float px_right, float py_bottom,
                   float r, float g, float b, float a);

// 5x7 glyphs for uppercase letters + digits + '.' + '-' + space
static const std::unordered_map<char, std::array<uint8_t,7>> GLYPH_5x7 = {
    {' ', {0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    // Letters
    {'A', {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'B', {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
    {'C', {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}},
    {'D', {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}},
    {'E', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}},
    {'G', {0x0E,0x11,0x10,0x17,0x11,0x11,0x0E}},
    {'H', {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'I', {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F}},
    {'J', {0x07,0x02,0x02,0x02,0x12,0x12,0x0C}},
    {'K', {0x11,0x12,0x14,0x18,0x14,0x12,0x11}},
    {'L', {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},
    {'N', {0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
    {'O', {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}},
    {'P', {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
    {'R', {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
    {'S', {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}},
    {'T', {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
    {'U', {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}},
    {'Y', {0x11,0x0A,0x04,0x04,0x04,0x04,0x04}},
    // Digits
    {'0', {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}},
    {'1', {0x04,0x0C,0x14,0x04,0x04,0x04,0x1F}},
    {'2', {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}},
    {'3', {0x1F,0x02,0x04,0x06,0x01,0x11,0x0E}},
    {'4', {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}},
    {'5', {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
    {'6', {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}},
    {'7', {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
    {'8', {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}},
    {'9', {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},
    // Punctuation
    {'.', {0x00,0x00,0x00,0x00,0x00,0x00,0x04}},
    {'-', {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}},
};

// ---------------- Decls ----------------
void framebuffer_size_callback(GLFWwindow*, int, int);
void mouse_button_callback(GLFWwindow*, int, int, int);
void processInput(GLFWwindow*);

void setupNodesAndLines();
unsigned int compileProgram(const char* vs, const char* fs);
void setupMapBuffers();
void drawHighlightedPath();
std::pair<std::vector<int>, double> findShortestPath(int start, int end);

// Path info labels (distance/time/cost) — now HUD bottom-left
void drawPathInfoLabels();

// Terminal/Windows helpers
std::string absolutePathToAsset(const std::string& fileName);
std::string toFileURI(const std::string& absPath);
void listLinksForNode(int nodeIndex, bool showMissingHints=true);
void openImageForNodeByIndex(int nodeIndex, int imgIdx);
void openCurrentImageForNode(int nodeIndex);
void openAllImagesForNode(int nodeIndex);
int  wrapIndex(int i, int n);

// ---------------- Main ----------------
int main(){
    if (!glfwInit()){ std::cout<<"Failed to init GLFW\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(windowW, windowH, "Map (click two nodes for shortest path)", nullptr, nullptr);
    if (!window){ std::cout<<"Failed to create window\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::cout<<"Failed to init GLAD\n"; return -1;
    }

    setupNodesAndLines();
    shaderProgram = compileProgram(vertexShaderSource, fragmentShaderSource);
    setupMapBuffers();
    setupTextBuffers(); // labels init

    glEnable(GL_PROGRAM_POINT_SIZE);
    glPointSize(15.0f);
    glLineWidth(3.0f);

    // Enable alpha blending for the HUD panel
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    std::cout << "Controls:\n";
    std::cout << "  • Left-click two nodes: computes and highlights the shortest path.\n";
    std::cout << "  • Bottom-left HUD shows: distance (small), estimated time (small), travel cost (small).\n";
    std::cout << "  • Image keys (after clicking a node): 1..9, ],[, O,A,L.\n";
    std::cout << "Put your JPEGs in .\\assets and list them in nodeImages at the top of main.cpp.\n\n";

    while (!glfwWindowShouldClose(window)){
        processInput(window);

        glClearColor(0.12f,0.14f,0.17f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        int nodeColor = glGetUniformLocation(shaderProgram, "nodeColor");
        int alphaLoc  = glGetUniformLocation(shaderProgram, "uAlpha");

        // draw edges
        glUniform3f(nodeColor, 1.0f,1.0f,1.0f);
        glUniform1f(alphaLoc, 1.0f);
        glBindVertexArray(VAO_lines);
        glDrawArrays(GL_LINES, 0, (GLsizei)lines.size());

        // draw highlighted path
        if (!pathIndices.empty()) {
            drawHighlightedPath();
        }

        // draw nodes
        glUniform3f(nodeColor, 0.9f,0.55f,0.20f);
        glUniform1f(alphaLoc, 1.0f);
        glBindVertexArray(VAO_nodes);
        glDrawArrays(GL_POINTS, 0, (GLsizei)nodes.size());

        // draw labels above nodes (original size)
        drawAllNodeLabels();

        // draw HUD (after everything else so it overlays cleanly)
        if (!pathIndices.empty()) {
            drawPathInfoLabels();
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1,&VAO_nodes);
    glDeleteBuffers(1,&VBO_nodes);
    glDeleteVertexArrays(1,&VAO_lines);
    glDeleteBuffers(1,&VBO_lines);
    glDeleteVertexArrays(1,&VAO_text);
    glDeleteBuffers(1,&VBO_text);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}

// ---------------- Callbacks ----------------
void framebuffer_size_callback(GLFWwindow*, int w, int h){
    windowW = (w<=0?1:w);
    windowH = (h<=0?1:h);
    glViewport(0, 0, windowW, windowH);
}

void processInput(GLFWwindow* window){
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window,true);

    // Edge-triggered key handling to avoid repeat spam while holding a key
    static std::unordered_map<int,bool> wasDown;

    auto onPress = [&](int key){
        int state = glfwGetKey(window, key);
        bool down = (state == GLFW_PRESS);
        bool fired = false;
        if (down && !wasDown[key]) { fired = true; }
        wasDown[key] = down;
        return fired;
    };

    if (lastClickedNodeIndex != -1){
        for (int k=GLFW_KEY_1; k<=GLFW_KEY_9; ++k){
            if (onPress(k)){
                int idx = k - GLFW_KEY_1; // 0-based
                openImageForNodeByIndex(lastClickedNodeIndex, idx);
            }
        }
        if (onPress(GLFW_KEY_RIGHT_BRACKET)){
            auto &cur = curImageIdxForNode[lastClickedNodeIndex];
            const auto &list = nodeImages[nodes[lastClickedNodeIndex].name];
            if (!list.empty()){
                cur = wrapIndex(cur+1, (int)list.size());
                std::cout << "Current image: #" << (cur+1) << "\n";
                openCurrentImageForNode(lastClickedNodeIndex);
            }
        }
        if (onPress(GLFW_KEY_LEFT_BRACKET)){
            auto &cur = curImageIdxForNode[lastClickedNodeIndex];
            const auto &list = nodeImages[nodes[lastClickedNodeIndex].name];
            if (!list.empty()){
                cur = wrapIndex(cur-1, (int)list.size());
                std::cout << "Current image: #" << (cur+1) << "\n";
                openCurrentImageForNode(lastClickedNodeIndex);
            }
        }
        if (onPress(GLFW_KEY_O)){
            openCurrentImageForNode(lastClickedNodeIndex);
        }
        if (onPress(GLFW_KEY_A)){
            openAllImagesForNode(lastClickedNodeIndex);
        }
        if (onPress(GLFW_KEY_L)){
            listLinksForNode(lastClickedNodeIndex);
        }
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int){
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;

    double xpos, ypos; glfwGetCursorPos(window, &xpos, &ypos);
    int width, height; glfwGetWindowSize(window, &width, &height);

    // Convert to NDC
    float opengl_x =  (float)xpos / (width/2.0f) - 1.0f;
    float opengl_y =  1.0f - (float)ypos / (height/2.0f);

    // Hit test
    int clicked = -1;
    const float R = 0.05f;
    for (size_t i=0;i<nodes.size();++i){
        float dx = opengl_x - nodes[i].x;
        float dy = opengl_y - nodes[i].y;
        if (std::sqrt(dx*dx + dy*dy) < R){ clicked=(int)i; break; }
    }
    if (clicked == -1) return;

    lastClickedNodeIndex = clicked;

    if (selectedNodeIndex1 == -1){
        selectedNodeIndex1 = clicked;
        pathIndices.clear();
        std::cout<<"Starting from: "<<nodes[selectedNodeIndex1].name<<"\n";
    } else if (clicked != selectedNodeIndex1){
        selectedNodeIndex2 = clicked;
        std::cout<<"Destination is: "<<nodes[selectedNodeIndex2].name<<"\n";

        auto res = findShortestPath(selectedNodeIndex1, selectedNodeIndex2);
        pathIndices = res.first;
        totalPathDistance = res.second;

        if (!pathIndices.empty()){
            std::cout<<"Path: ";
            for (size_t i=0;i<pathIndices.size();++i){
                std::cout<<nodes[pathIndices[i]].name<<(i+1<pathIndices.size()? " -> ":"\n");
            }
            std::cout<<"Total distance: "<<totalPathDistance<<"\n";
        } else {
            std::cout<<"No path found between "<<nodes[selectedNodeIndex1].name<<" and "<<nodes[selectedNodeIndex2].name<<"\n";
        }
        selectedNodeIndex1 = -1;
        selectedNodeIndex2 = -1;
    }
}

// ---------------- Graph setup & rendering ----------------
void setupNodesAndLines(){
    nodes = {
        Node(-0.7f,  0.6f, "Rangpur"),
        Node( 0.2f,  0.8f, "Sylhet"),
        Node(-0.3f, -0.4f, "Khulna"),
        Node( 0.8f, -0.7f, "Chittagong"),
        Node( 0.0f,  0.0f, "Dhaka"),
        Node(-0.9f, -0.2f, "Rajshahi"),
        Node( 0.6f,  0.1f, "Barishal")
    };
    lines = { 0,1, 0,2, 1,3, 2,3, 0,4, 1,4, 2,4, 3,4, 5,0, 5,2, 6,1, 6,3 };
    linesWithWeights = {
        WeightedLine(0,1,500.0), WeightedLine(0,2,512.0),  WeightedLine(1,3,363.0),
        WeightedLine(2,3,442.0), WeightedLine(0,4,294.0),  WeightedLine(1,4,240.0),
        WeightedLine(2,4,222.0),  WeightedLine(3,4,257.0),  WeightedLine(5,0,217.0),
        WeightedLine(5,2,255.0), WeightedLine(6,1,402.0),  WeightedLine(6,3,243.0)
    };
    adjacencyList.resize(nodes.size());
    for (const auto& e: linesWithWeights){
        adjacencyList[e.start].push_back({e.end, e.weight});
        adjacencyList[e.end].push_back({e.start, e.weight});
    }
}

unsigned int compileProgram(const char* vs, const char* fs){
    auto compile = [](GLenum type, const char* src){
        GLuint sh = glCreateShader(type);
        glShaderSource(sh,1,&src,nullptr);
        glCompileShader(sh);
        GLint ok=0; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok){ char log[1024]; glGetShaderInfoLog(sh,1024,nullptr,log);
            std::cerr<<"Shader compile error:\n"<<log<<"\n"; }
        return sh;
    };
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p);
    GLint ok=0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok){ char log[1024]; glGetProgramInfoLog(p,1024,nullptr,log);
        std::cerr<<"Program link error:\n"<<log<<"\n"; }
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

void setupMapBuffers(){
    // nodes
    std::vector<float> pts; pts.reserve(nodes.size()*3);
    for (auto& n: nodes){ pts.insert(pts.end(), {n.x,n.y,0.0f}); }
    glGenVertexArrays(1,&VAO_nodes);
    glGenBuffers(1,&VBO_nodes);
    glBindVertexArray(VAO_nodes);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_nodes);
    glBufferData(GL_ARRAY_BUFFER, pts.size()*sizeof(float), pts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    // lines
    std::vector<float> segs; segs.reserve(lines.size()*3);
    for (int idx : lines){
        segs.insert(segs.end(), {nodes[idx].x, nodes[idx].y, 0.0f});
    }
    glGenVertexArrays(1,&VAO_lines);
    glGenBuffers(1,&VBO_lines);
    glBindVertexArray(VAO_lines);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_lines);
    glBufferData(GL_ARRAY_BUFFER, segs.size()*sizeof(float), segs.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER,0);
    glBindVertexArray(0);
}

void drawHighlightedPath(){
    if (pathIndices.size()<2) return;
    std::vector<float> verts;
    verts.reserve((pathIndices.size()-1)*6);
    for (size_t i=0;i+1<pathIndices.size();++i){
        auto &a = nodes[pathIndices[i]];
        auto &b = nodes[pathIndices[i+1]];
        verts.insert(verts.end(), {a.x,a.y,0.0f, b.x,b.y,0.0f});
    }
    GLuint vao,vbo;
    glGenVertexArrays(1,&vao);
    glGenBuffers(1,&vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STREAM_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    int nodeColor = glGetUniformLocation(shaderProgram,"nodeColor");
    int alphaLoc  = glGetUniformLocation(shaderProgram,"uAlpha");
    glUniform3f(nodeColor, 0.0f,1.0f,0.0f);
    glUniform1f(alphaLoc, 1.0f);
    glDrawArrays(GL_LINES, 0, (GLsizei)(verts.size()/3));

    glDeleteBuffers(1,&vbo);
    glDeleteVertexArrays(1,&vao);
}

// Dijkstra
std::pair<std::vector<int>, double> findShortestPath(int start, int end){
    if (start==end) return {{start}, 0.0};
    std::map<int,double> dist;
    std::map<int,int> parent;
    for (size_t i=0;i<nodes.size();++i){ dist[(int)i]=std::numeric_limits<double>::infinity(); parent[(int)i]=-1; }
    dist[start]=0.0;

    auto cmp=[&](int a,int b){ return dist[a]>dist[b]; };
    std::priority_queue<int,std::vector<int>,decltype(cmp)> pq(cmp);
    pq.push(start);

    while(!pq.empty()){
        int u=pq.top(); pq.pop();
        if (u==end) break;
        for (auto& e: adjacencyList[u]){
            int v=e.first; double w=e.second;
            if (dist[u]+w<dist[v]){
                dist[v]=dist[u]+w; parent[v]=u; pq.push(v);
            }
        }
    }
    if (dist[end]==std::numeric_limits<double>::infinity()) return {{},0.0};

    std::vector<int> path;
    for (int cur=end; cur!=-1; cur=parent[cur]) path.push_back(cur);
    std::reverse(path.begin(), path.end());
    return {path, dist[end]};
}

// ---------------- Path info labels (HUD) ----------------
static inline float segLen(float ax, float ay, float bx, float by){
    float dx=bx-ax, dy=by-ay; return std::sqrt(dx*dx+dy*dy);
}

void setupTextBuffers(){
    glGenVertexArrays(1, &VAO_text);
    glGenBuffers(1, &VBO_text);
    glBindVertexArray(VAO_text);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_text);
    glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_STREAM_DRAW); // will resize per draw
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// Original-size node labels (NDC anchored above node)
void drawLabelAtNDC(float ndc_x, float ndc_y, const std::string& text){
    // Render at original size
    const float scale = 1.0f;
    // Place text slightly above the point (using existing helper)
    const float extraAbovePx = 18.0f;
    // Use same scaled path that converts pixel -> NDC internally
    // Implemented via the "WithExtraAboveScaled" helper:
    // (Reimplemented inline to minimize dependencies)
    // For clarity, reuse the scaled variant:
    auto drawLabelAtNDCWithExtraAboveScaled = [](float ndc_x_, float ndc_y_, const std::string& text_, float extraAbovePx_, float scale_){
        if (!VAO_text || !VBO_text) return;
        if (scale_ <= 0.0f) scale_ = 1.0f;

        const float dot_base = 2.0f, gap_base = 1.0f, charGap_base = 2.0f;
        const float dot = dot_base * scale_, gap = gap_base * scale_, charGap = charGap_base * scale_;
        const float charW = 5*dot + 4*gap;
        const float charH = 7*dot + 6*gap;

        const int textLen = (int)text_.size();
        const float textW = textLen * (charW + charGap) - (textLen?charGap:0);
        const float textH = charH;

        const float pxX = ((ndc_x_ + 1.0f)*0.5f)*windowW;
        const float pxY = ((1.0f - ndc_y_)*0.5f)*windowH;

        float baseX = pxX - textW * 0.5f;
        float baseY = pxY - (extraAbovePx_) - textH;

        std::vector<float> verts;
        verts.reserve(text_.size()*5*7*6*3);

        auto addRect = [&](float x, float y, float w, float h){
            verts.insert(verts.end(), { x, y, 0.0f,  x+w, y, 0.0f,  x+w, y+h, 0.0f });
            verts.insert(verts.end(), { x, y, 0.0f,  x+w, y+h, 0.0f,  x, y+h, 0.0f });
        };

        auto addDot = [&](float x, float y){ addRect(x,y,dot,dot); };

        for (size_t i=0;i<text_.size();++i){
            char c = (char)std::toupper((unsigned char)text_[i]);
            auto it = GLYPH_5x7.find(c);
            const auto& rows = (it!=GLYPH_5x7.end()) ? it->second : GLYPH_5x7.at(' ');
            float cx = baseX + (float)i*(charW+charGap);
            for (int rrow=0;rrow<7;++rrow){
                uint8_t bits=rows[rrow];
                for (int col=0; col<5; ++col){
                    if (bits & (1<<(4-col))){
                        float x=cx + col*(dot+gap);
                        float y=baseY + rrow*(dot+gap);
                        addDot(x,y);
                    }
                }
            }
        }

        for (size_t i=0;i<verts.size(); i+=3){
            float px=verts[i], py=verts[i+1];
            verts[i]   =  2.0f * px / windowW - 1.0f;
            verts[i+1] = -2.0f * py / windowH + 1.0f;
        }

        glUseProgram(shaderProgram);
        int nodeColor = glGetUniformLocation(shaderProgram, "nodeColor");
        int alphaLoc  = glGetUniformLocation(shaderProgram, "uAlpha");
        glUniform3f(nodeColor, 1.0f, 1.0f, 1.0f);
        glUniform1f(alphaLoc, 1.0f);

        glBindVertexArray(VAO_text);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_text);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)), verts.data(), GL_STREAM_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size()/3));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    };

    drawLabelAtNDCWithExtraAboveScaled(ndc_x, ndc_y, text, extraAbovePx, scale);
}

void drawAllNodeLabels(){
    for (const auto& n : nodes){
        drawLabelAtNDC(n.x, n.y, n.name);
    }
}

// Draw text at an exact pixel position (top-left anchor), scaled.
void drawLabelAtPixelScaled(float px_left, float py_top, const std::string& text, float scale){
    if (!VAO_text || !VBO_text) return;
    if (scale <= 0.0f) scale = 1.0f;

    // Base pixel metrics (original size)
    const float dot_base = 2.0f;
    const float gap_base = 1.0f;
    const float charGap_base = 2.0f;

    // Scaled metrics
    const float dot = dot_base * scale;
    const float gap = gap_base * scale;
    const float charGap = charGap_base * scale;
    const float charW = 5*dot + 4*gap;
    const float charH = 7*dot + 6*gap;

    std::vector<float> verts;
    verts.reserve(text.size()*5*7*6*3);

    auto addRect = [&](float x, float y, float w, float h){
        verts.insert(verts.end(), { x, y, 0.0f,  x+w, y, 0.0f,  x+w, y+h, 0.0f });
        verts.insert(verts.end(), { x, y, 0.0f,  x+w, y+h, 0.0f,  x, y+h, 0.0f });
    };
    auto addDot = [&](float x, float y){ addRect(x,y,dot,dot); };

    for (size_t i=0;i<text.size();++i){
        char c = (char)std::toupper((unsigned char)text[i]);
        auto it = GLYPH_5x7.find(c);
        const auto& rows = (it!=GLYPH_5x7.end()) ? it->second : GLYPH_5x7.at(' ');
        float cx = px_left + (float)i*(charW+charGap);
        for (int rrow=0;rrow<7;++rrow){
            uint8_t bits=rows[rrow];
            for (int col=0; col<5; ++col){
                if (bits & (1<<(4-col))){
                    float x=cx + col*(dot+gap);
                    float y=py_top + rrow*(dot+gap);
                    addDot(x,y);
                }
            }
        }
    }

    // Convert pixels (origin top-left) to NDC
    for (size_t i=0;i<verts.size(); i+=3){
        float px=verts[i], py=verts[i+1];
        verts[i]   =  2.0f * px / windowW - 1.0f;
        verts[i+1] = -2.0f * py / windowH + 1.0f;
    }

    glUseProgram(shaderProgram);
    int nodeColor = glGetUniformLocation(shaderProgram, "nodeColor");
    int alphaLoc  = glGetUniformLocation(shaderProgram, "uAlpha");
    glUniform3f(nodeColor, 1.0f, 1.0f, 1.0f); // white text
    glUniform1f(alphaLoc, 1.0f);

    glBindVertexArray(VAO_text);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_text);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)), verts.data(), GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size()/3));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

float measureTextWidthPx(const std::string& text, float scale){
    const float dot_base = 2.0f, gap_base = 1.0f, charGap_base = 2.0f;
    const float dot = dot_base * scale;
    const float gap = gap_base * scale;
    const float charGap = charGap_base * scale;
    const float charW = 5*dot + 4*gap;
    if (text.empty()) return 0.0f;
    return (float)text.size() * (charW + charGap) - charGap;
}

void drawPixelRect(float px_left, float py_top, float px_right, float py_bottom,
                   float r, float g, float b, float a){
    // Build two triangles in NDC
    float x0 =  2.0f * px_left  / windowW - 1.0f;
    float y0 = -2.0f * py_top   / windowH + 1.0f;
    float x1 =  2.0f * px_right / windowW - 1.0f;
    float y1 = -2.0f * py_bottom/ windowH + 1.0f;

    float verts[] = {
        x0,y0,0.0f,  x1,y0,0.0f,  x1,y1,0.0f,
        x0,y0,0.0f,  x1,y1,0.0f,  x0,y1,0.0f
    };

    GLuint vao,vbo;
    glGenVertexArrays(1,&vao);
    glGenBuffers(1,&vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    glUseProgram(shaderProgram);
    int nodeColor = glGetUniformLocation(shaderProgram, "nodeColor");
    int alphaLoc  = glGetUniformLocation(shaderProgram, "uAlpha");
    glUniform3f(nodeColor, r,g,b);
    glUniform1f(alphaLoc, a);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDeleteBuffers(1,&vbo);
    glDeleteVertexArrays(1,&vao);
}

void drawPathInfoLabels(){
    if (pathIndices.size()<2) return;

    // 1) Distance (km, integer)
    std::ostringstream ssDist;
    ssDist << (int)std::round(totalPathDistance) << "KM";
    std::string distTxt = ssDist.str();

    // 2) Estimated time (decimal hours, 1dp)
    double hours = (kSpeedUnitsPerHour>0.0) ? (totalPathDistance / kSpeedUnitsPerHour) : 0.0;
    std::ostringstream ssTime;
    ssTime << std::fixed << std::setprecision(1) << hours << "H";
    std::string timeTxt = ssTime.str();

    // 3) Cost
    double cost = totalPathDistance * kCostPerUnit;
    std::ostringstream ssCost;
    ssCost << "COST " << kCurrency << " " << std::fixed << std::setprecision(1) << cost;
    std::string costTxt = ssCost.str();

    const float scale = kPathInfoTextScale;
    const float lineGapPx = kHudLineGapBasePx * scale;

    // Character height (px) at this scale
    const float dot_base = 2.0f, gap_base = 1.0f;
    const float dot = dot_base * scale;
    const float gap = gap_base * scale;
    const float charH = 7*dot + 6*gap;

    // We stack upwards from bottom-left: Distance (bottom), Time (above), Cost (top)
    float y_dist_top = windowH - kHudMarginBottomPx - charH;
    float y_time_top = y_dist_top - (charH + lineGapPx);
    float y_cost_top = y_time_top - (charH + lineGapPx);

    // Measure widths to size the background panel
    float wDist = measureTextWidthPx(distTxt, scale);
    float wTime = measureTextWidthPx(timeTxt, scale);
    float wCost = measureTextWidthPx(costTxt, scale);
    float maxW  = std::max(wDist, std::max(wTime, wCost));

    float panelLeft   = kHudMarginLeftPx - kHudPaddingPx;
    float panelRight  = kHudMarginLeftPx + maxW + kHudPaddingPx;
    float panelTop    = y_cost_top - kHudPaddingPx;
    float panelBottom = y_dist_top + charH + kHudPaddingPx;

    // Draw semi-transparent panel first
    drawPixelRect(panelLeft, panelTop, panelRight, panelBottom,
                  kHudPanelGray, kHudPanelGray, kHudPanelGray, kHudPanelAlpha);

    // Draw text on top
    drawLabelAtPixelScaled(kHudMarginLeftPx, y_cost_top, costTxt, scale);
    drawLabelAtPixelScaled(kHudMarginLeftPx, y_time_top, timeTxt, scale);
    drawLabelAtPixelScaled(kHudMarginLeftPx, y_dist_top, distTxt, scale);
}

// ---------------- Terminal/Windows helpers ----------------
std::string absolutePathToAsset(const std::string& fileName){
    // 1. Get the current working directory (e.g., C:/.../graphics/)
    std::filesystem::path currentDir = std::filesystem::current_path();

    // 2. Construct the final path directly: [Root] / assets / [fileName]
    // This is correct because 'assets' is a direct child of the root.
    std::filesystem::path p = currentDir / "assets" / fileName;

    // Use weakly_canonical to get a clean, resolved path
    return std::filesystem::weakly_canonical(p).string();
}

std::string toFileURI(const std::string& absPath){
    std::string uri = "file:///";
    for (char c : absPath) uri.push_back(c=='\\' ? '/' : c);
    return uri;
}

void listLinksForNode(int nodeIndex, bool showMissingHints){
    const std::string& name = nodes[nodeIndex].name;
    auto it = nodeImages.find(name);
    if (it == nodeImages.end() || it->second.empty()){
        std::cout << "No images listed for node '"<<name<<"'. Add entries to nodeImages.\n";
        return;
    }
    const auto& imgs = it->second;
    std::cout << "Images for node '"<<name<<"' ("<<imgs.size()<<"):\n";
    for (size_t i=0;i<imgs.size();++i){
        std::string abs = absolutePathToAsset(imgs[i]);
        bool exists = std::filesystem::exists(abs);
        std::cout << "  " << (i+1) << ") " << toFileURI(abs);
        if (!exists && showMissingHints) std::cout << "   [missing file]";
        std::cout << "\n";
    }
    std::cout << "Keys: 1..9 open specific | ]/[ next/prev | O open current | A open all | L list again\n";
}

void openImageForNodeByIndex(int nodeIndex, int imgIdx){
#ifdef _WIN32
    const std::string& name = nodes[nodeIndex].name;
    auto it = nodeImages.find(name);
    if (it == nodeImages.end() || imgIdx < 0 || imgIdx >= (int)it->second.size()){
        std::cout << "Invalid image index.\n";
        return;
    }
    std::string abs = absolutePathToAsset(it->second[imgIdx]);
    HINSTANCE r = ShellExecuteA(nullptr, "open", abs.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)r <= 32){
        std::cout << "Failed to open image (code " << (INT_PTR)r << "). Path:\n  " << abs << "\n";
    } else {
        curImageIdxForNode[nodeIndex] = imgIdx;
        std::cout << "Opened image #" << (imgIdx+1) << "\n";
    }
#else
    (void)nodeIndex; (void)imgIdx;
#endif
}

void openCurrentImageForNode(int nodeIndex){
#ifdef _WIN32
    const std::string& name = nodes[nodeIndex].name;
    auto it = nodeImages.find(name);
    if (it == nodeImages.end() || it->second.empty()){
        std::cout << "No images configured for this node.\n";
        return;
    }
    int &cur = curImageIdxForNode[nodeIndex];
    cur = wrapIndex(cur, (int)it->second.size());
    openImageForNodeByIndex(nodeIndex, cur);
#else
    (void)nodeIndex;
#endif
}

void openAllImagesForNode(int nodeIndex){
#ifdef _WIN32
    const std::string& name = nodes[nodeIndex].name;
    auto it = nodeImages.find(name);
    if (it == nodeImages.end() || it->second.empty()){
        std::cout << "No images configured for this node.\n";
        return;
    }
    std::cout << "Opening all images ("<<it->second.size()<<")...\n";
    for (size_t i=0;i<it->second.size();++i){
        openImageForNodeByIndex(nodeIndex, (int)i);
    }
#else
    (void)nodeIndex;
#endif
}

int wrapIndex(int i, int n){
    if (n<=0) return 0;
    int r = i % n;
    if (r < 0) r += n;
    return r;
}

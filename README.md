# Pathfinder
🗺️ Bangladesh City Route Visualizer
Interactive OpenGL Map with Shortest Path & Image Gallery

This project is an interactive C++ OpenGL application that visualizes major cities of Bangladesh as nodes in a 2D map. Users can click two cities to compute the shortest route between them using Dijkstra’s algorithm, view travel details (distance, time, and cost), and open images for each city from the project’s assets folder.

🚀 Features
🟢 Interactive Map

Cities are displayed as nodes on a 2D OpenGL-rendered plane.

Weighted edges represent real distances between cities.

🛣️ Shortest Path Algorithm

Uses Dijkstra’s algorithm to compute the shortest path between two selected cities.

The path is highlighted in green on the map.

🧾 Travel Information (HUD)

A clean bottom-left HUD displays:

Total Distance (KM)

Estimated Time (Hrs)

Travel Cost (BDT)
All rendered with a custom bitmap 5×7 font.

🖼️ City Image Viewer

Each city has multiple images stored in the assets/ folder.
Keyboard controls allow:

1–9 → Open specific image

] / [ → Next / previous image

O → Open current image

A → Open all images

L → List image file paths in terminal

🎨 Modern Rendering

OpenGL 3.3 Core Profile (GLFW + GLAD)

Custom vertex + fragment shaders

Custom text rendering pipeline

Smooth alpha-blended HUD panel

📁 Project Structure
graphics/
│
├── assets/              # All city image files used in the program
├── include/             # Header files (if any)
├── lib/                 # External libraries
├── src/
│   └── main.cpp         # Main application source code
│
├── cutable.exe          # Compiled executable
└── glfw3.dll            # GLFW runtime DLL

🏗️ How It Works
1️⃣ Graph Setup

Cities are stored in Node objects with:

X/Y map coordinates

City name

Edges are stored in WeightedLine with distances.
An adjacency list is built for Dijkstra’s algorithm.

2️⃣ Rendering

Nodes drawn as GL_POINTS

Edges drawn as GL_LINES

Labels rendered above nodes

Path drawn using temporary VBOs

HUD uses pixel-perfect text rendering

3️⃣ User Interaction

Mouse click selects nodes

Keyboard opens images or cycles through them

Terminal logs activities like selected node, path, current image, etc.

🧭 Controls
Action	Control
Quit program	ESC
Select two cities	Left-click
Open image #n	1–9
Next/previous image	] / [
Open current image	O
Open all images	A
List all image links	L
🛠️ Requirements

C++17 or later

GLFW

GLAD

OpenGL 3.3+ compatible GPU

Windows OS (for image-opening functionality)

▶️ Running the Program

Place your image files inside the assets/ folder.

Ensure all filenames match those specified in nodeImages in main.cpp.

Build the project with a C++ compiler linking GLFW and GLAD.

Run the generated executable:

./cutable.exe

📌 Customization
📍 Add or modify nodes/cities

Update the list in:

std::vector<Node> nodes;

🖼️ Add images for any city

Edit:

std::unordered_map<std::string, std::vector<std::string>> nodeImages;

💰 Change travel cost / speed

Modify constants:

kSpeedUnitsPerHour
kCostPerUnit

📜 License

This project is for academic and personal use.

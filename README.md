# Tron Recognizer (1982) - Legacy OpenGL Implementation

this is a temp readme, currently woirking on a blog-like version

## Overview

This project is a programmatic recreation of the iconic **Recognizer** from the 1982 film *Tron*. Unlike modern rendering pipelines that rely on imported meshes and shaders, this project builds the 3D model entirely from raw vertex data using C and Legacy OpenGL (GLUT/GLU).

The goal is to replicate the specific "backlit animation" aesthetic of the original film: luminous wireframes over solid, matte-shaded geometric volumes, simulating the look of early CGI without using standard OpenGL lighting engines.

## Key Features

  * **Procedural Animation:** A calculated flight path simulating a landing sequence (Arc on the XZ plane, linear descent on Y) with synchronized Pitch, Yaw, and Roll.
  * **Custom "Toon" Shading:** Instead of using `glEnable(GL_LIGHTING)`, lighting is calculated manually via vector math (Dot Product) to achieve a binary "On/Off" lighting threshold, mimicking the stark contrast of the movie cells.
  * **Dual-Pass Rendering:** The scene is rendered twice per frame to achieve the visual style:
    1.  **Pass 1 (Solid):** Draws the geometry with calculated shading (Dark Green/Black) using polygon offsets to prevent z-fighting.
    2.  **Pass 2 (Wireframe):** Draws the edges in bright Red, unaffected by lighting.
  * **Robust Normal Calculation:** Features a custom geometry engine that calculates face normals dynamically using cross products and centroid logic to determine outward orientation.

## Technical Implementation & Intentions

This project is an exercise in understanding the fundamentals of 3D graphics mathematics.

### 1\. Geometry Extrusion Engine

The model is defined as a series of 2D profiles (arrays of X/Y coordinates). The engine extrudes these profiles into 3D volumes defined by a front depth (`Z1`) and a rear depth (`Z2`). The system automatically generates the front face, back face, and connecting side walls.

### 2\. Manual Lighting Mathematics

To maintain total control over the retro look, standard OpenGL lighting is disabled.

  * **Rotations:** A custom `rotateVector` function manually applies Pitch, Yaw, and Roll matrices to the surface normals.
  * **Lighting:** The lighting is determined by the Dot Product between the rotated normal and a fixed Light Vector.
  * **Threshold:** A hard threshold (`dot > 0.2f`) determines if a face is colored or black, creating the flat-shaded look.


### 3\. Debugging Tools

The code includes a built-in debug mode (`SHOW_NORMALS = 1`) which renders:

  * **Yellow Spikes:** Visualizing the normal vector of every face to verify orientation.
  * **Blue Ray:** Visualizing the direction of the light source.

## Controls

  * **Right Arrow:** Advance animation frame.
  * **Left Arrow:** Rewind animation frame.
  * **Mouse:** (Debug mode only) Rotate view to inspect geometry.

## Compilation

The project depends on `freeglut`, `glu`, and `math`.

**Linux (GCC):**

```bash
gcc tron_recognizer.c -o recognizer -lGL -lGLU -lglut -lm
./recognizer
```
-----

*This project is a tribute to the computer graphics pioneers of the 1982 film TRON.*

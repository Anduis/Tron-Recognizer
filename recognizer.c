#include <GL/glut.h>
#include <GL/glu.h>
#include <math.h>
#include <stdio.h>
#define FLIP(x) (1 << x)

typedef struct
{
  float x, y, z;
} Vec3;

// --- MATH CONSTANTS ---
#define M_PI 3.14159265358979323846
#define RAD2DEG (180.0f / M_PI)

// --- ANIMATION SETTINGS ---
const int FPS = 24;
const int TOTAL_SECONDS = 4;
const int MAX_FRAMES = FPS * TOTAL_SECONDS;

// --- PATH CONFIGURATION ---
float radius = 5.0f;
float startY = 4.0f;
float endY = -3.0f;

// Rotations
float startPitch = M_PI / 3;
float startYaw = M_PI / 2;
float startRoll = M_PI / 6;
float endPitch = -M_PI / 8;
float endYaw = 0.0f;
float endRoll = 0.0f;

// --- LIGHT SOURCE ---
Vec3 light = {1.0f, 1.0f, 1.0f};

// --- GLOBAL STATE ---
int currentFrame = 0;
int isWireframe = 0;
float curX, curY, curZ;
float curPitch, curYaw, curRoll;

// Colors
float colorWire[] = {1.0f, 0.0f, 0.0f};
float colorSolid[] = {0.133f, 0.275f, 0.024f}; // #224606

// --- TESSELATOR OBJECT ---
GLUtesselator *tess;

// --- MATH HELPERS ---

float lerp(float start, float end, float t)
{
  return start + t * (end - start);
}

void normalize(Vec3 *v)
{
  float len = sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
  if (len > 0.001f)
  {
    v->x /= len;
    v->y /= len;
    v->z /= len;
  }
}

float dotProduct(Vec3 a, Vec3 b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 crossProduct(Vec3 a, Vec3 b)
{
  Vec3 res;
  res.x = a.y * b.z - a.z * b.y;
  res.y = a.z * b.x - a.x * b.z;
  res.z = a.x * b.y - a.y * b.x;
  return res;
}

Vec3 rotateVector(Vec3 v, float pitch, float yaw, float roll)
{
  Vec3 res = v;
  float c, s;
  if (pitch != 0.0f)
  {
    c = cos(pitch);
    s = sin(pitch);
    float y = res.y * c - res.z * s;
    float z = res.y * s + res.z * c;
    res.y = y;
    res.z = z;
  }
  if (yaw != 0.0f)
  {
    c = cos(yaw);
    s = sin(yaw);
    float x = res.x * c + res.z * s;
    float z = -res.x * s + res.z * c;
    res.x = x;
    res.z = z;
  }
  if (roll != 0.0f)
  {
    c = cos(roll);
    s = sin(roll);
    float x = res.x * c - res.y * s;
    float y = res.x * s + res.y * c;
    res.x = x;
    res.y = y;
  }
  return res;
}

// --- COLOR LOGIC ---
void setFaceColor(Vec3 normal)
{
  if (isWireframe)
  {
    glColor3fv(colorWire);
    return;
  }
  Vec3 rotNorm = rotateVector(normal, curPitch, curYaw, curRoll);
  float dot = dotProduct(rotNorm, light);
  if (dot > 0.2f)
    glColor3fv(colorSolid);
  else
    glColor3f(0.0f, 0.0f, 0.0f);
}

// --- TESSELATOR CALLBACKS ---
#ifndef CALLBACK
#define CALLBACK
#endif
void CALLBACK tessBeginCB(GLenum which) { glBegin(which); }
void CALLBACK tessEndCB() { glEnd(); }
void CALLBACK tessVertexCB(const GLvoid *data) { glVertex3dv((const GLdouble *)data); }
void CALLBACK tessCombineCB(GLdouble coords[3], GLdouble *vertex_data[4], GLfloat weight[4], GLdouble **outData)
{
  GLdouble *vertex = (GLdouble *)malloc(3 * sizeof(GLdouble));
  vertex[0] = coords[0];
  vertex[1] = coords[1];
  vertex[2] = coords[2];
  *outData = vertex;
}

// --- GEOMETRY ENGINE ---

// Calculates the center point of the entire shape
Vec3 calcCentroid(float data[][4], int n)
{
  Vec3 c = {0.0f, 0.0f, 0.0f};
  for (int i = 0; i < n; i++)
  {
    c.x += data[i][0];
    c.y += data[i][1];
    // Average the Z depth for the center
    c.z += (data[i][2] + data[i][3]) / 2.0f;
  }
  c.x /= n;
  c.y /= n;
  c.z /= n;
  return c;
}

// It calculates the normal, then checks if it points towards the center of the object.
// If it does, it flips it. This ensures ALL faces point OUTWARD.
Vec3 calcOutwardNormal(Vec3 p1, Vec3 p2, Vec3 p3, Vec3 centroid, Vec3 faceCenter)
{
  // 1. Calculate Basic Normal
  Vec3 u = {p2.x - p1.x, p2.y - p1.y, p2.z - p1.z};
  Vec3 v = {p3.x - p1.x, p3.y - p1.y, p3.z - p1.z};
  Vec3 n = crossProduct(u, v);
  normalize(&n);

  // 2. Vector from Centroid to Face Center
  Vec3 outDir = {faceCenter.x - centroid.x, faceCenter.y - centroid.y, faceCenter.z - centroid.z};

  // 3. Check Direction (Dot Product)
  // If dot product is negative, Normal and OutDir are opposite -> Normal points Inward -> FLIP IT
  if (dotProduct(n, outDir) < 0.0f)
  {
    n.x = -n.x;
    n.y = -n.y;
    n.z = -n.z;
  }

  return n;
}

void drawPolyPart(float data[][4], int n, int flipMask)
{

  // 1. Calculate Centroid of this specific part
  Vec3 centroid = calcCentroid(data, n);

  // 2. FRONT FACE (Z1)
  if (n >= 3)
  {
    Vec3 p0 = {data[0][0], data[0][1], data[0][2]};
    Vec3 p1 = {data[1][0], data[1][1], data[1][2]};
    Vec3 p2 = {data[2][0], data[2][1], data[2][2]};

    // Calculate approx center of front face
    Vec3 faceCenter = {0, 0, 0};
    for (int i = 0; i < n; i++)
    {
      faceCenter.x += data[i][0];
      faceCenter.y += data[i][1];
      faceCenter.z += data[i][2];
    }
    faceCenter.x /= n;
    faceCenter.y /= n;
    faceCenter.z /= n;

    setFaceColor(calcOutwardNormal(p0, p1, p2, centroid, faceCenter));
  }

  if (isWireframe)
  {
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < n; i++)
      glVertex3f(data[i][0], data[i][1], data[i][2]);
    glEnd();
  }
  else
  {
    gluTessBeginPolygon(tess, NULL);
    gluTessBeginContour(tess);
    GLdouble *verts = (GLdouble *)malloc(n * 3 * sizeof(GLdouble));
    for (int i = 0; i < n; i++)
    {
      verts[i * 3 + 0] = data[i][0];
      verts[i * 3 + 1] = data[i][1];
      verts[i * 3 + 2] = data[i][2];
      gluTessVertex(tess, &verts[i * 3], &verts[i * 3]);
    }
    gluTessEndContour(tess);
    gluTessEndPolygon(tess);
    free(verts);
  }

  // 3. BACK FACE (Z2)
  if (n >= 3)
  {
    Vec3 p0 = {data[0][0], data[0][1], data[0][3]};
    Vec3 p1 = {data[1][0], data[1][1], data[1][3]};
    Vec3 p2 = {data[2][0], data[2][1], data[2][3]};

    Vec3 faceCenter = {0, 0, 0};
    for (int i = 0; i < n; i++)
    {
      faceCenter.x += data[i][0];
      faceCenter.y += data[i][1];
      faceCenter.z += data[i][3];
    }
    faceCenter.x /= n;
    faceCenter.y /= n;
    faceCenter.z /= n;

    setFaceColor(calcOutwardNormal(p0, p1, p2, centroid, faceCenter));
  }

  if (isWireframe)
  {
    glBegin(GL_LINE_LOOP);
    for (int i = n - 1; i >= 0; i--)
      glVertex3f(data[i][0], data[i][1], data[i][3]);
    glEnd();
  }
  else
  {
    gluTessBeginPolygon(tess, NULL);
    gluTessBeginContour(tess);
    GLdouble *verts = (GLdouble *)malloc(n * 3 * sizeof(GLdouble));
    for (int i = n - 1; i >= 0; i--)
    {
      int idx = i;
      verts[i * 3 + 0] = data[idx][0];
      verts[i * 3 + 1] = data[idx][1];
      verts[i * 3 + 2] = data[idx][3];
      gluTessVertex(tess, &verts[i * 3], &verts[i * 3]);
    }
    gluTessEndContour(tess);
    gluTessEndPolygon(tess);
    free(verts);
  }

  // 4. SIDES
  for (int i = 0; i < n; i++)
  {
    int next = (i + 1) % n;
    Vec3 p1 = {data[i][0], data[i][1], data[i][2]};
    Vec3 p2 = {data[i][0], data[i][1], data[i][3]};
    Vec3 p3 = {data[next][0], data[next][1], data[next][3]};
    Vec3 p4 = {data[next][0], data[next][1], data[next][2]};

    // Calculate center of this specific side quad
    Vec3 faceCenter;
    faceCenter.x = (p1.x + p2.x + p3.x + p4.x) / 4.0f;
    faceCenter.y = (p1.y + p2.y + p3.y + p4.y) / 4.0f;
    faceCenter.z = (p1.z + p2.z + p3.z + p4.z) / 4.0f;

    // Calculate normal using the Centroid logic
    Vec3 n = calcOutwardNormal(p1, p2, p4, centroid, faceCenter);
    if ((flipMask >> i) & 1)
    {
      n.x = -n.x;
      n.y = -n.y;
      n.z = -n.z;
    }
    setFaceColor(n);

    glBegin(isWireframe ? GL_LINE_LOOP : GL_QUADS);
    glVertex3f(p1.x, p1.y, p1.z);
    glVertex3f(p2.x, p2.y, p2.z);
    glVertex3f(p3.x, p3.y, p3.z);
    glVertex3f(p4.x, p4.y, p4.z);
    glEnd();
  }
}

// --- DATA IMPORT ---
void drawRecognizer()
{
  float leftEar[][4] = {{-0.6000, 0.6000, 0.1545, -0.1717}, {-0.1932, 0.6000, 0.1545, -0.1717}, {-0.0861, 0.8484, 0.0034, -0.1394}};
  drawPolyPart(leftEar, 3, 0);
  float leftChin[][4] = {{-0.1932, 0.6000, 0.1545, -0.1717}, {-0.0852, 0.6000, 0.1545, -0.1717}, {-0.1492, 0.7020, 0.0925, -0.1584}};
  drawPolyPart(leftChin, 3, 0);
  float botChin[][4] = {{-0.0852, 0.6000, 0.1545, -0.1717}, {0.0852, 0.6000, 0.1545, -0.1717}, {0.1048, 0.6312, 0.1355, -0.1676}, {-0.1048, 0.6312, 0.1355, -0.1676}};
  drawPolyPart(botChin, 4, 0);
  float botHead[][4] = {{-0.1176, 0.6516, 0.1231, -0.1400}, {0.1176, 0.6516, 0.1231, -0.1400}, {0.1492, 0.7020, 0.1600, -0.1434}, {-0.1492, 0.7020, 0.1600, -0.1434}};
  drawPolyPart(botHead, 4, 0);
  float midHead[][4] = {{-0.1492, 0.7020, 0.1600, -0.1434}, {0.1492, 0.7020, 0.1600, -0.1434}, {0.0861, 0.8484, 0.0319, -0.1244}, {-0.0861, 0.8484, 0.0319, -0.1244}};
  drawPolyPart(midHead, 4, 0);
  float lowEyes[][4] = {{-0.0861, 0.8484, 0.0319, -0.1244}, {0.0861, 0.8484, 0.0319, -0.1244}, {0.0938, 0.8808, 0.0560, -0.1301}, {-0.0938, 0.8808, 0.0560, -0.1301}};
  drawPolyPart(lowEyes, 4, 0);
  float upEyes[][4] = {{-0.0938, 0.8808, 0.0560, -0.1301}, {0.0938, 0.8808, 0.0560, -0.1301}, {0.0861, 0.9144, 0.0319, -0.1054}, {-0.0861, 0.9144, 0.0319, -0.1054}};
  drawPolyPart(upEyes, 4, 0);
  float rightChin[][4] = {{0.6000, 0.6000, 0.1545, -0.1717}, {0.1932, 0.6000, 0.1545, -0.1717}, {0.0861, 0.8484, 0.0034, -0.1394}};
  drawPolyPart(rightChin, 3, 0);
  float rightEar[][4] = {{0.1932, 0.6000, 0.1545, -0.1717}, {0.0852, 0.6000, 0.1545, -0.1717}, {0.1492, 0.7020, 0.0925, -0.1584}};
  drawPolyPart(rightEar, 3, 0);
  float leftWing[][4] = {{-1.20, 0.56, 0.216, -0.206}, {-1.20, 0.50, 0.223, -0.206}, {-0.72, 0.50, 0.223, -0.206}, {-0.53, 0.27, 0.251, -0.206}, {-0.03, 0.27, 0.251, -0.206}, {-0.03, 0.34, 0.242, -0.206}, {-0.27, 0.34, 0.242, -0.206}, {-0.40, 0.50, 0.223, -0.206}, {-0.03, 0.50, 0.223, -0.206}, {-0.03, 0.56, 0.216, -0.206}};
  drawPolyPart(leftWing, 10, FLIP(1) | FLIP(5) | FLIP(7));
  float rightWing[][4] = {{1.20, 0.56, 0.216, -0.206}, {1.20, 0.50, 0.223, -0.206}, {0.72, 0.50, 0.223, -0.206}, {0.53, 0.27, 0.251, -0.206}, {0.03, 0.27, 0.251, -0.206}, {0.03, 0.34, 0.242, -0.206}, {0.27, 0.34, 0.242, -0.206}, {0.40, 0.50, 0.223, -0.206}, {0.03, 0.50, 0.223, -0.206}, {0.03, 0.56, 0.216, -0.206}};
  drawPolyPart(rightWing, 10, FLIP(1) | FLIP(5) | FLIP(7));
  float encBlock[][4] = {{-0.30, 0.46, 0.228, -0.206}, {-0.23, 0.38, 0.238, -0.206}, {0.23, 0.38, 0.238, -0.206}, {0.30, 0.46, 0.228, -0.206}};
  drawPolyPart(encBlock, 4, 0);
  float leftBlock[][4] = {{-1.10, 0.46, 0.172, -0.172}, {-1.10, 0.24, 0.172, -0.172}, {-0.90, 0.24, 0.172, -0.172}, {-0.90, 0.46, 0.172, -0.172}};
  drawPolyPart(leftBlock, 4, 0);
  float rightBlock[][4] = {{1.10, 0.46, 0.172, -0.172}, {1.10, 0.24, 0.172, -0.172}, {0.90, 0.24, 0.172, -0.172}, {0.90, 0.46, 0.172, -0.172}};
  drawPolyPart(rightBlock, 4, 0);
  float floatL[][4] = {{-0.87, 0.40, 0.172, -0.172}, {-0.87, 0.32, 0.172, -0.172}, {-0.70, 0.32, 0.172, -0.172}, {-0.70, 0.40, 0.172, -0.172}};
  drawPolyPart(floatL, 4, 0);
  float floatR[][4] = {{0.87, 0.40, 0.172, -0.172}, {0.87, 0.32, 0.172, -0.172}, {0.70, 0.32, 0.172, -0.172}, {0.70, 0.40, 0.172, -0.172}};
  drawPolyPart(floatR, 4, 0);
  float midBlock[][4] = {{-0.23, 0.25, 0.206, -0.137}, {-0.23, 0.21, 0.172, -0.172}, {0.23, 0.21, 0.172, -0.172}, {0.23, 0.25, 0.206, -0.137}};
  drawPolyPart(midBlock, 4, 0);
  float midBar[][4] = {{-1.10, 0.19, 0.216, -0.216}, {-1.10, 0.12, 0.172, -0.216}, {1.10, 0.12, 0.172, -0.216}, {1.10, 0.19, 0.216, -0.216}};
  drawPolyPart(midBar, 4, 0);
  float botBlock[][4] = {{-0.23, 0.09, 0.134, -0.161}, {-0.18, 0.05, 0.110, -0.137}, {0.18, 0.05, 0.110, -0.137}, {0.23, 0.09, 0.134, -0.161}};
  drawPolyPart(botBlock, 4, 0);
  float leftFoot[][4] = {{-1.10, 0.07, 0.172, -0.172}, {-1.10, -0.90, 0.172, -0.172}, {-0.60, -0.90, 0.172, -0.172}, {-0.90, -0.73, 0.172, -0.172}, {-0.90, 0.07, 0.172, -0.172}};
  drawPolyPart(leftFoot, 5, FLIP(2));
  float rightFoot[][4] = {{1.10, 0.07, 0.172, -0.172}, {1.10, -0.90, 0.172, -0.172}, {0.60, -0.90, 0.172, -0.172}, {0.90, -0.73, 0.172, -0.172}, {0.90, 0.07, 0.172, -0.172}};
  drawPolyPart(rightFoot, 5, FLIP(2));
}

// --- MAIN LOOP ---
void calculatePath(int frame)
{
  float t = (float)frame / (float)(MAX_FRAMES - 1);
  float theta = lerp(M_PI, 0.0f, t);
  curX = radius * cos(theta);
  curZ = radius * sin(-theta);
  curY = lerp(startY, endY, t);
  curPitch = lerp(startPitch, endPitch, t);
  curYaw = lerp(startYaw, endYaw, t);
  curRoll = lerp(startRoll, endRoll, t);
}

void renderFrameCounter()
{
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D(0, 800, 0, 600);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glColor3f(1.0f, 1.0f, 1.0f);
  glRasterPos2i(20, 570);
  char buffer[20];
  sprintf(buffer, "%03d", currentFrame);
  for (char *c = buffer; *c != '\0'; c++)
    glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
}

void drawLightVector()
{
  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);
  glLineWidth(3.0f);

  float scale = 8.0f;

  glBegin(GL_LINES);
  glColor3f(0.0f, 0.5f, 1.0f);
  glVertex3f(0.0f, 0.0f, 0.0f);
  glVertex3f(light.x * scale, light.y * scale, light.z * scale);
  glEnd();

  glEnable(GL_DEPTH_TEST);
  glLineWidth(1.0f);
}


void display()
{
  calculatePath(currentFrame);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  gluLookAt(0.0, 5.0, 15.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

  drawLightVector();

  if (!isWireframe)
  {
    glBegin(GL_LINES);
    glColor3f(0.15f, 0.15f, 0.15f);
    for (float i = -10; i <= 10; i += 1.0f)
    {
      glVertex3f(i, -3, 10);
      glVertex3f(i, -3, -10);
      glVertex3f(10, -3, i);
      glVertex3f(-10, -3, i);
    }
    glEnd();
  }

  glPushMatrix();
  glTranslatef(curX, curY, curZ);
  glRotatef(curRoll * RAD2DEG, 0.0f, 0.0f, 1.0f);
  glRotatef(curYaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
  glRotatef(curPitch * RAD2DEG, 1.0f, 0.0f, 0.0f);

  isWireframe = 0;
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.0, 1.0);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  drawRecognizer();
  glDisable(GL_POLYGON_OFFSET_FILL);

  isWireframe = 1;
  glLineWidth(2.0f);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  drawRecognizer();

  glPopMatrix();
  renderFrameCounter();
  glutSwapBuffers();
}

void specialKeys(int key, int x, int y)
{
  if (key == GLUT_KEY_RIGHT && currentFrame < MAX_FRAMES - 1)
    currentFrame++;
  else if (key == GLUT_KEY_LEFT && currentFrame > 0)
    currentFrame--;
  glutPostRedisplay();
}

void init()
{
  glClearColor(0.0, 0.0, 0.0, 1.0);
  glEnable(GL_DEPTH_TEST);
  tess = gluNewTess();
  gluTessCallback(tess, GLU_TESS_BEGIN, (void(CALLBACK *)())tessBeginCB);
  gluTessCallback(tess, GLU_TESS_END, (void(CALLBACK *)())tessEndCB);
  gluTessCallback(tess, GLU_TESS_VERTEX, (void(CALLBACK *)())tessVertexCB);
  gluTessCallback(tess, GLU_TESS_COMBINE, (void(CALLBACK *)())tessCombineCB);
  gluTessProperty(tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO);
}

void reshape(int w, int h)
{
  if (h == 0)
    h = 1;
  float aspect = (float)w / (float)h;
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45.0, aspect, 0.1, 100.0);
  glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char **argv)
{
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(800, 800);
  glutCreateWindow("Tron: Recognizer");
  init();
  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutSpecialFunc(specialKeys);
  glutMainLoop();
  gluDeleteTess(tess);
  return 0;
}

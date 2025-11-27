#include <GL/glut.h>
#include <GL/glu.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define M_PI 3.14159265358979323846
#define FLIP(x) (1 << x)

typedef struct
{
  float x, y, z;
} Vec3;

int SHOW_NORMALS = 0;

float rotX = 0.0f;
float rotY = 0.0f;
int lastMouseX, lastMouseY;
int isDragging = 0;

Vec3 light = {1.0f, 1.0f, 0.0f};

float colorWire[] = {1.0f, 0.0f, 0.0f};
float colorSolid[] = {0.133f, 0.275f, 0.024f};

GLUtesselator *tess;

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

Vec3 calcCentroid(float data[][4], int n)
{
  Vec3 c = {0, 0, 0};
  for (int i = 0; i < n; i++)
  {
    c.x += data[i][0];
    c.y += data[i][1];
    c.z += (data[i][2] + data[i][3]) / 2.0f;
  }
  c.x /= n;
  c.y /= n;
  c.z /= n;
  return c;
}

void setLitColor(Vec3 normal)
{
  float radX = rotX * (M_PI / 180.0f);
  float radY = rotY * (M_PI / 180.0f);

  float ny = normal.y * cos(radX) - normal.z * sin(radX);
  float nz = normal.y * sin(radX) + normal.z * cos(radX);
  normal.y = ny;
  normal.z = nz;

  float nx = normal.x * cos(radY) + normal.z * sin(radY);
  nz = -normal.x * sin(radY) + normal.z * cos(radY);
  normal.x = nx;
  normal.z = nz;

  float dot = dotProduct(normal, light);

  if (dot > 0.2f)
    glColor3fv(colorSolid);
  else
    glColor3f(0.0f, 0.0f, 0.0f);
}

Vec3 calcOutwardNormal(Vec3 p1, Vec3 p2, Vec3 p3, Vec3 centroid, Vec3 faceCenter)
{
  Vec3 u = {p2.x - p1.x, p2.y - p1.y, p2.z - p1.z};
  Vec3 v = {p3.x - p1.x, p3.y - p1.y, p3.z - p1.z};
  Vec3 n = crossProduct(u, v);
  normalize(&n);

  Vec3 outward = {faceCenter.x - centroid.x, faceCenter.y - centroid.y, faceCenter.z - centroid.z};

  if (dotProduct(n, outward) < 0.0f)
  {
    n.x = -n.x;
    n.y = -n.y;
    n.z = -n.z;
  }
  return n;
}

void drawDebugNormal(Vec3 center, Vec3 n)
{
  if (!SHOW_NORMALS)
    return;
  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);

  glBegin(GL_LINES);
  glColor3f(1.0f, 1.0f, 0.0f);
  glVertex3f(center.x, center.y, center.z);
  glVertex3f(center.x + n.x * 0.4f, center.y + n.y * 0.4f, center.z + n.z * 0.4f);
  glEnd();

  glEnable(GL_DEPTH_TEST);
}

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

void drawPolyPart(float data[][4], int n, int isWire, int flipMask)
{
  Vec3 centroid = calcCentroid(data, n);

  if (n >= 3)
  {
    Vec3 p0 = {data[0][0], data[0][1], data[0][2]};
    Vec3 p1 = {data[1][0], data[1][1], data[1][2]};
    Vec3 p2 = {data[2][0], data[2][1], data[2][2]};
    Vec3 fc = {0, 0, 0};
    for (int i = 0; i < n; i++)
    {
      fc.x += data[i][0];
      fc.y += data[i][1];
      fc.z += data[i][2];
    }
    fc.x /= n;
    fc.y /= n;
    fc.z /= n;

    Vec3 norm = calcOutwardNormal(p0, p1, p2, centroid, fc);
    if (isWire)
    {
      glColor3fv(colorWire);
      glBegin(GL_LINE_LOOP);
      for (int i = 0; i < n; i++)
        glVertex3f(data[i][0], data[i][1], data[i][2]);
      glEnd();
      drawDebugNormal(fc, norm);
    }
    else
    {
      setLitColor(norm);
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
  }

  if (n >= 3)
  {
    Vec3 p0 = {data[0][0], data[0][1], data[0][3]};
    Vec3 p1 = {data[1][0], data[1][1], data[1][3]};
    Vec3 p2 = {data[2][0], data[2][1], data[2][3]};
    Vec3 fc = {0, 0, 0};
    for (int i = 0; i < n; i++)
    {
      fc.x += data[i][0];
      fc.y += data[i][1];
      fc.z += data[i][3];
    }
    fc.x /= n;
    fc.y /= n;
    fc.z /= n;

    Vec3 norm = calcOutwardNormal(p0, p1, p2, centroid, fc);
    if (isWire)
    {
      glColor3fv(colorWire);
      glBegin(GL_LINE_LOOP);
      for (int i = n - 1; i >= 0; i--)
        glVertex3f(data[i][0], data[i][1], data[i][3]);
      glEnd();
      drawDebugNormal(fc, norm);
    }
    else
    {
      setLitColor(norm);
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
  }

  for (int i = 0; i < n; i++)
  {
    int next = (i + 1) % n;
    Vec3 p1 = {data[i][0], data[i][1], data[i][2]};
    Vec3 p2 = {data[i][0], data[i][1], data[i][3]};
    Vec3 p3 = {data[next][0], data[next][1], data[next][3]};
    Vec3 p4 = {data[next][0], data[next][1], data[next][2]};

    Vec3 fc;
    fc.x = (p1.x + p2.x + p3.x + p4.x) / 4.0f;
    fc.y = (p1.y + p2.y + p3.y + p4.y) / 4.0f;
    fc.z = (p1.z + p2.z + p3.z + p4.z) / 4.0f;

    Vec3 norm = calcOutwardNormal(p1, p2, p4, centroid, fc);

    if ((flipMask >> i) & 1)
    {
      norm.x = -norm.x;
      norm.y = -norm.y;
      norm.z = -norm.z;
    }

    if (isWire)
    {
      glColor3fv(colorWire);
      glBegin(GL_LINE_LOOP);
      glVertex3f(p1.x, p1.y, p1.z);
      glVertex3f(p2.x, p2.y, p2.z);
      glVertex3f(p3.x, p3.y, p3.z);
      glVertex3f(p4.x, p4.y, p4.z);
      glEnd();
      drawDebugNormal(fc, norm);
    }
    else
    {
      setLitColor(norm);
      glBegin(GL_QUADS);
      glVertex3f(p1.x, p1.y, p1.z);
      glVertex3f(p2.x, p2.y, p2.z);
      glVertex3f(p3.x, p3.y, p3.z);
      glVertex3f(p4.x, p4.y, p4.z);
      glEnd();
    }
  }
}

void drawRecognizer(int isWire)
{
  float leftEar[][4] = {{-0.6000, 0.6000, 0.1545, -0.1717}, {-0.1932, 0.6000, 0.1545, -0.1717}, {-0.0861, 0.8484, 0.0034, -0.1394}};
  drawPolyPart(leftEar, 3, isWire, 0);
  float leftChin[][4] = {{-0.1932, 0.6000, 0.1545, -0.1717}, {-0.0852, 0.6000, 0.1545, -0.1717}, {-0.1492, 0.7020, 0.0925, -0.1584}};
  drawPolyPart(leftChin, 3, isWire, 0);
  float botChin[][4] = {{-0.0852, 0.6000, 0.1545, -0.1717}, {0.0852, 0.6000, 0.1545, -0.1717}, {0.1048, 0.6312, 0.1355, -0.1676}, {-0.1048, 0.6312, 0.1355, -0.1676}};
  drawPolyPart(botChin, 4, isWire, 0);
  float botHead[][4] = {{-0.1176, 0.6516, 0.1231, -0.1400}, {0.1176, 0.6516, 0.1231, -0.1400}, {0.1492, 0.7020, 0.1600, -0.1434}, {-0.1492, 0.7020, 0.1600, -0.1434}};
  drawPolyPart(botHead, 4, isWire, 0);
  float midHead[][4] = {{-0.1492, 0.7020, 0.1600, -0.1434}, {0.1492, 0.7020, 0.1600, -0.1434}, {0.0861, 0.8484, 0.0319, -0.1244}, {-0.0861, 0.8484, 0.0319, -0.1244}};
  drawPolyPart(midHead, 4, isWire, 0);
  float lowEyes[][4] = {{-0.0861, 0.8484, 0.0319, -0.1244}, {0.0861, 0.8484, 0.0319, -0.1244}, {0.0938, 0.8808, 0.0560, -0.1301}, {-0.0938, 0.8808, 0.0560, -0.1301}};
  drawPolyPart(lowEyes, 4, isWire, 0);
  float upEyes[][4] = {{-0.0938, 0.8808, 0.0560, -0.1301}, {0.0938, 0.8808, 0.0560, -0.1301}, {0.0861, 0.9144, 0.0319, -0.1054}, {-0.0861, 0.9144, 0.0319, -0.1054}};
  drawPolyPart(upEyes, 4, isWire, 0);
  float rightChin[][4] = {{0.6000, 0.6000, 0.1545, -0.1717}, {0.1932, 0.6000, 0.1545, -0.1717}, {0.0861, 0.8484, 0.0034, -0.1394}};
  drawPolyPart(rightChin, 3, isWire, 0);
  float rightEar[][4] = {{0.1932, 0.6000, 0.1545, -0.1717}, {0.0852, 0.6000, 0.1545, -0.1717}, {0.1492, 0.7020, 0.0925, -0.1584}};
  drawPolyPart(rightEar, 3, isWire, 0);
  float leftWing[][4] = {{-1.20, 0.56, 0.216, -0.206}, {-1.20, 0.50, 0.223, -0.206}, {-0.72, 0.50, 0.223, -0.206}, {-0.53, 0.27, 0.251, -0.206}, {-0.03, 0.27, 0.251, -0.206}, {-0.03, 0.34, 0.242, -0.206}, {-0.27, 0.34, 0.242, -0.206}, {-0.40, 0.50, 0.223, -0.206}, {-0.03, 0.50, 0.223, -0.206}, {-0.03, 0.56, 0.216, -0.206}};
  drawPolyPart(leftWing, 10, isWire, FLIP(1) | FLIP(5) | FLIP(7));
  float rightWing[][4] = {{1.20, 0.56, 0.216, -0.206}, {1.20, 0.50, 0.223, -0.206}, {0.72, 0.50, 0.223, -0.206}, {0.53, 0.27, 0.251, -0.206}, {0.03, 0.27, 0.251, -0.206}, {0.03, 0.34, 0.242, -0.206}, {0.27, 0.34, 0.242, -0.206}, {0.40, 0.50, 0.223, -0.206}, {0.03, 0.50, 0.223, -0.206}, {0.03, 0.56, 0.216, -0.206}};
  drawPolyPart(rightWing, 10, isWire, FLIP(1) | FLIP(5) | FLIP(7));
  float encBlock[][4] = {{-0.30, 0.46, 0.228, -0.206}, {-0.23, 0.38, 0.238, -0.206}, {0.23, 0.38, 0.238, -0.206}, {0.30, 0.46, 0.228, -0.206}};
  drawPolyPart(encBlock, 4, isWire, 0);
  float leftBlock[][4] = {{-1.10, 0.46, 0.172, -0.172}, {-1.10, 0.24, 0.172, -0.172}, {-0.90, 0.24, 0.172, -0.172}, {-0.90, 0.46, 0.172, -0.172}};
  drawPolyPart(leftBlock, 4, isWire, 0);
  float rightBlock[][4] = {{1.10, 0.46, 0.172, -0.172}, {1.10, 0.24, 0.172, -0.172}, {0.90, 0.24, 0.172, -0.172}, {0.90, 0.46, 0.172, -0.172}};
  drawPolyPart(rightBlock, 4, isWire, 0);
  float floatL[][4] = {{-0.87, 0.40, 0.172, -0.172}, {-0.87, 0.32, 0.172, -0.172}, {-0.70, 0.32, 0.172, -0.172}, {-0.70, 0.40, 0.172, -0.172}};
  drawPolyPart(floatL, 4, isWire, 0);
  float floatR[][4] = {{0.87, 0.40, 0.172, -0.172}, {0.87, 0.32, 0.172, -0.172}, {0.70, 0.32, 0.172, -0.172}, {0.70, 0.40, 0.172, -0.172}};
  drawPolyPart(floatR, 4, isWire, 0);
  float midBlock[][4] = {{-0.23, 0.25, 0.206, -0.137}, {-0.23, 0.21, 0.172, -0.172}, {0.23, 0.21, 0.172, -0.172}, {0.23, 0.25, 0.206, -0.137}};
  drawPolyPart(midBlock, 4, isWire, 0);
  float midBar[][4] = {{-1.10, 0.19, 0.216, -0.216}, {-1.10, 0.12, 0.172, -0.216}, {1.10, 0.12, 0.172, -0.216}, {1.10, 0.19, 0.216, -0.216}};
  drawPolyPart(midBar, 4, isWire, 0);
  float botBlock[][4] = {{-0.23, 0.09, 0.134, -0.161}, {-0.18, 0.05, 0.110, -0.137}, {0.18, 0.05, 0.110, -0.137}, {0.23, 0.09, 0.134, -0.161}};
  drawPolyPart(botBlock, 4, isWire, 0);
  float leftFoot[][4] = {{-1.10, 0.07, 0.172, -0.172}, {-1.10, -0.90, 0.172, -0.172}, {-0.60, -0.90, 0.172, -0.172}, {-0.90, -0.73, 0.172, -0.172}, {-0.90, 0.07, 0.172, -0.172}};
  drawPolyPart(leftFoot, 5, isWire, FLIP(2));
  float rightFoot[][4] = {{1.10, 0.07, 0.172, -0.172}, {1.10, -0.90, 0.172, -0.172}, {0.60, -0.90, 0.172, -0.172}, {0.90, -0.73, 0.172, -0.172}, {0.90, 0.07, 0.172, -0.172}};
  drawPolyPart(rightFoot, 5, isWire, FLIP(2));
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
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();

  gluLookAt(0.0, 0.0, 10.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

  drawLightVector();

  glRotatef(rotX, 1.0f, 0.0f, 0.0f);
  glRotatef(rotY, 0.0f, 1.0f, 0.0f);

  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.0, 1.0);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  drawRecognizer(0);
  glDisable(GL_POLYGON_OFFSET_FILL);

  glLineWidth(2.0f);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  drawRecognizer(1);

  glutSwapBuffers();
}

void mouse(int button, int state, int x, int y)
{
  if (button == GLUT_LEFT_BUTTON)
  {
    if (state == GLUT_DOWN)
    {
      isDragging = 1;
      lastMouseX = x;
      lastMouseY = y;
    }
    else
    {
      isDragging = 0;
    }
  }
}

void motion(int x, int y)
{
  if (isDragging)
  {
    rotY += (x - lastMouseX) * 0.5f;
    rotX += (y - lastMouseY) * 0.5f;
    lastMouseX = x;
    lastMouseY = y;
    glutPostRedisplay();
  }
}

void init()
{
  glClearColor(0.2, 0.2, 0.2, 1.0);
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
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45, (float)w / h, 1, 100);
  glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char **argv)
{
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(800, 600);
  glutCreateWindow("Tron: Recognizer Debug");
  init();
  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutMouseFunc(mouse);
  glutMotionFunc(motion);
  glutMainLoop();
  gluDeleteTess(tess);
  return 0;
}

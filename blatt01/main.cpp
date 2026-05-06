#include <iostream>
#include <vector>

#include <GL/glew.h>
//#include <GL/gl.h> // OpenGL header not necessary, included by GLEW
#include <GL/freeglut.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "GLSLProgram.h"
#include "GLTools.h"
#include <iomanip>

// Standard window width
const int WINDOW_WIDTH  = 640;
// Standard window height
const int WINDOW_HEIGHT = 480;
// GLUT window id/handle
int glutID = 0;

cg::GLSLProgram program;

glm::mat4x4 view;
glm::mat4x4 projection;

float zNear = 0.1f;
float zFar  = 100.0f;

/*
Struct to hold data for object rendering.
*/
class Object
{
public:
  inline Object ()
    : vao(0),
      positionBuffer(0),
      colorBuffer(0),
      indexBuffer(0)
  {}

  inline ~Object () { // GL context must exist on destruction
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &indexBuffer);
    glDeleteBuffers(1, &colorBuffer);
    glDeleteBuffers(1, &positionBuffer);
  }

  GLuint vao;        // vertex-array-object ID
  
  GLuint positionBuffer; // ID of vertex-buffer: position
  GLuint colorBuffer;    // ID of vertex-buffer: color
  
  GLuint indexBuffer;    // ID of index-buffer
  
  glm::mat4x4 model; // model matrix
};

Object triangle;
Object quad;

bool convertMode = false;
// Color mode enum
enum class ColorMode { RGB, CMY, HSV };

ColorMode currentColorMode = ColorMode::RGB;
bool editingTriangle = true; // true = Dreieck, false = Quad

// Current colors (stored as RGB for OpenGL)
glm::vec3 triangleColors[3] = {
  {1.0f, 0.0f, 0.0f},
  {0.0f, 1.0f, 0.0f},
  {0.0f, 0.0f, 1.0f}
};
glm::vec3 quadColors[4] = {
  {1.0f, 0.0f, 0.0f},
  {0.0f, 1.0f, 1.0f},
  {0.0f, 1.0f, 0.0f},
  {0.0f, 0.0f, 1.0f}
};

// Input state
bool waitingForInput = false;
int  inputStep       = 0;   // 0=erster, 1=zweiter, 2=dritter Wert
float inputValues[3] = {};
int  inputVertexIndex = 0;  // welcher Vertex gerade editiert wird

// RGB -> CMY
glm::vec3 rgbToCmy(glm::vec3 rgb) {
  return glm::vec3(1.0f - rgb.r, 1.0f - rgb.g, 1.0f - rgb.b);
}

// RGB -> HSV
glm::vec3 rgbToHsv(glm::vec3 rgb) {
  float r = rgb.r, g = rgb.g, b = rgb.b;
  float max = glm::max(r, glm::max(g, b));
  float min = glm::min(r, glm::min(g, b));
  float delta = max - min;

  float h = 0, s = 0, v = max;

  if (delta > 0.0001f) {
    s = delta / max;
    if      (max == r) h = 60.0f * fmod((g - b) / delta, 6.0f);
    else if (max == g) h = 60.0f * ((b - r) / delta + 2.0f);
    else               h = 60.0f * ((r - g) / delta + 4.0f);
    if (h < 0) h += 360.0f;
  }
  return glm::vec3(h, s, v);
}

void printColorEquivalents(glm::vec3 rgb) {
  glm::vec3 cmy = rgbToCmy(rgb);
  glm::vec3 hsv = rgbToHsv(rgb);

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "Die Eingabe enspricht :" << std::endl;
  std::cout << "  RGB: R=" << rgb.r << " G=" << rgb.g << " B=" << rgb.b << std::endl;
  std::cout << "  CMY: C=" << cmy.r << " M=" << cmy.g << " Y=" << cmy.b << std::endl;
  std::cout << "  HSV: H=" << hsv.r << " S=" << hsv.g << " V=" << hsv.b << std::endl;
}


void renderTriangle()
{
  // Create mvp.
  glm::mat4x4 mvp = projection * view * triangle.model;
  
  // Bind the shader program and set uniform(s).
  program.use();
  program.setUniform("mvp", mvp);
  
  // Bind vertex array object so we can render the 1 triangle.
  glBindVertexArray(triangle.vao);
  glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, 0);
  glBindVertexArray(0);
}

void renderQuad()
{
  // Create mvp.
  glm::mat4x4 mvp = projection * view * quad.model;
  
  // Bind the shader program and set uniform(s).
  program.use();
  program.setUniform("mvp", mvp);
  
  // Bind vertex array object so we can render the 2 triangles.
  glBindVertexArray(quad.vao);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
  glBindVertexArray(0);
}

void initTriangle()
{
  // Construct triangle. These vectors can go out of scope after we have send all data to the graphics card.
  const std::vector<glm::vec3> vertices = { glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec3(1.0f, -1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f) };
  const std::vector<glm::vec3> colors   = { glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) };
  const std::vector<GLushort>  indices  = { 0, 1, 2 };

  GLuint programId = program.getHandle();
  GLuint pos;

  // Step 0: Create vertex array object.
  glGenVertexArrays(1, &triangle.vao);
  glBindVertexArray(triangle.vao);
  
  // Step 1: Create vertex buffer object for position attribute and bind it to the associated "shader attribute".
  glGenBuffers(1, &triangle.positionBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, triangle.positionBuffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
  
  // Bind it to position.
  pos = glGetAttribLocation(programId, "position");
  glEnableVertexAttribArray(pos);
  glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, 0);
  
  // Step 2: Create vertex buffer object for color attribute and bind it to...
  glGenBuffers(1, &triangle.colorBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, triangle.colorBuffer);
  glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec3), colors.data(), GL_STATIC_DRAW);
  
  // Bind it to color.
  pos = glGetAttribLocation(programId, "color");
  glEnableVertexAttribArray(pos);
  glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, 0);
  
  // Step 3: Create vertex buffer object for indices. No binding needed here.
  glGenBuffers(1, &triangle.indexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle.indexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLushort), indices.data(), GL_STATIC_DRAW);
  
  // Unbind vertex array object (back to default).
  glBindVertexArray(0);
  
  // Modify model matrix.
  triangle.model = glm::translate(glm::mat4(1.0f), glm::vec3(-1.25f, 0.0f, 0.0f));
}

void initQuad()
{
  // Construct triangle. These vectors can go out of scope after we have send all data to the graphics card.
  const std::vector<glm::vec3> vertices = { { -1.0f, 1.0f, 0.0f }, { -1.0, -1.0, 0.0 }, { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };
  const std::vector<glm::vec3> colors   = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0, 1.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
  const std::vector<GLushort>  indices  = { 0, 1, 2, 0, 2, 3 };

  GLuint programId = program.getHandle();
  GLuint pos;
  
  // Step 0: Create vertex array object.
  glGenVertexArrays(1, &quad.vao);
  glBindVertexArray(quad.vao);
  
  // Step 1: Create vertex buffer object for position attribute and bind it to the associated "shader attribute".
  glGenBuffers(1, &quad.positionBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, quad.positionBuffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
  
  // Bind it to position.
  pos = glGetAttribLocation(programId, "position");
  glEnableVertexAttribArray(pos);
  glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, 0);
  
  // Step 2: Create vertex buffer object for color attribute and bind it to...
  glGenBuffers(1, &quad.colorBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, quad.colorBuffer);
  glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec3), colors.data(), GL_STATIC_DRAW);
  
  // Bind it to color.
  pos = glGetAttribLocation(programId, "color");
  glEnableVertexAttribArray(pos);
  glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, 0);
  
  // Step 3: Create vertex buffer object for indices. No binding needed here.
  glGenBuffers(1, &quad.indexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad.indexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLushort), indices.data(), GL_STATIC_DRAW);
  
  // Unbind vertex array object (back to default).
  glBindVertexArray(0);
  
  // Modify model matrix.
  quad.model = glm::translate(glm::mat4(1.0f), glm::vec3(1.25f, 0.0f, 0.0f));
}

// CMY -> RGB
glm::vec3 cmyToRgb(float c, float m, float y) {
  return glm::vec3(1.0f - c, 1.0f - m, 1.0f - y);
}

// HSV -> RGB
glm::vec3 hsvToRgb(float h, float s, float v) {
  h = fmod(h, 360.0f);
  float c = v * s;
  float x = c * (1.0f - fabs(fmod(h / 60.0f, 2.0f) - 1.0f));
  float m = v - c;
  glm::vec3 rgb;
  if      (h < 60)  rgb = {c, x, 0};
  else if (h < 120) rgb = {x, c, 0};
  else if (h < 180) rgb = {0, c, x};
  else if (h < 240) rgb = {0, x, c};
  else if (h < 300) rgb = {x, 0, c};
  else              rgb = {c, 0, x};
  return rgb + glm::vec3(m);
}

// Farb-Buffer eines Objekts aktualisieren
void updateColorBuffer(Object& obj, const glm::vec3* colors, int count) {
  glBindBuffer(GL_ARRAY_BUFFER, obj.colorBuffer);
  glBufferData(GL_ARRAY_BUFFER, count * sizeof(glm::vec3), colors, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Eingabe verarbeiten und RGB-Farbe setzen
void applyInput() {
  glm::vec3 rgb;
  float a = inputValues[0], b = inputValues[1], c = inputValues[2];

  switch (currentColorMode) {
    case ColorMode::RGB: rgb = glm::vec3(a, b, c); break;
    case ColorMode::CMY: rgb = cmyToRgb(a, b, c);  break;
    case ColorMode::HSV: rgb = hsvToRgb(a, b, c);  break;
  }
  rgb = glm::clamp(rgb, 0.0f, 1.0f);

  if (convertMode) {
    // Nur umrechnen, nichts an den Objekten ändern
    std::cout << "Umrechnung:" << std::endl;
    printColorEquivalents(rgb);
    return;
  }

  // Farben auf Objekte anwenden
  if (editingTriangle) {
    for (int i = 0; i < 3; i++)
      triangleColors[i] = rgb;
    updateColorBuffer(triangle, triangleColors, 3);
  } else {
    for (int i = 0; i < 4; i++)
      quadColors[i] = rgb;
    updateColorBuffer(quad, quadColors, 4);
  }
  std::cout << "Farbe gesetzt!" << std::endl;
  printColorEquivalents(rgb);
}

void printInputPrompt() {
  const char* modeName = (currentColorMode == ColorMode::RGB) ? "RGB" :
                         (currentColorMode == ColorMode::CMY) ? "CMY" : "HSV";
  const char* obj = editingTriangle ? "Dreieck" : "Quad";
  const char* channels[3];

  if (currentColorMode == ColorMode::HSV)
    channels[0] = "H (0-360)", channels[1] = "S (0-1)", channels[2] = "V (0-1)";
  else if (currentColorMode == ColorMode::CMY)
    channels[0] = "C (0-1)",   channels[1] = "M (0-1)", channels[2] = "Y (0-1)";
  else
    channels[0] = "R (0-1)",   channels[1] = "G (0-1)", channels[2] = "B (0-1)";

  std::cout << channels[inputStep] << " eingeben: " << std::flush;
}

/*
 Initialization. Should return true if everything is ok and false if something went wrong.
 */
bool init()
{
  // OpenGL: Set "background" color and enable depth testing.
  glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  
  // Construct view matrix.
  glm::vec3 eye(0.0f, 0.0f, 4.0f);
  glm::vec3 center(0.0f, 0.0f, 0.0f);
  glm::vec3 up(0.0f, 1.0f, 0.0f);
  
  view = glm::lookAt(eye, center, up);
  
  // Create a shader program and set light direction.
  if (!program.compileShaderFromFile("shader/simple.vert", cg::GLSLShader::VERTEX)) {
    std::cerr << program.log();
    return false;
  }
  
  if (!program.compileShaderFromFile("shader/simple.frag", cg::GLSLShader::FRAGMENT)) {
    std::cerr << program.log();
    return false;
  }
  
  if (!program.link()) {
    std::cerr << program.log();
    return false;
  }

  // Create all objects.
  initTriangle();
  initQuad();
  
  return true;
}

/*
 Rendering.
 */
void render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	renderTriangle();
	renderQuad();
}

void glutDisplay ()
{
   render();
   glutSwapBuffers();
}

/*
 Resize callback.
 */
void glutResize (int width, int height)
{
  // Division by zero is bad...
  height = height < 1 ? 1 : height;
  glViewport(0, 0, width, height);
  
  // Construct projection matrix.
  projection = glm::perspective(45.0f, (float) width / height, zNear, zFar);
}

void printHelp() {
  std::cout << "========================================" << std::endl;
  std::cout << "       Aufgabenblatt 01 - Steuerung     " << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << std::endl;
  std::cout << "  OBJEKT WAEHLEN" << std::endl;
  std::cout << "    t .......... Dreieck auswaehlen" << std::endl;
  std::cout << "    q .......... Quad auswaehlen" << std::endl;
  std::cout << std::endl;
  std::cout << "  FARB-MODUS WAEHLEN" << std::endl;
  std::cout << "    r .......... RGB-Modus (R: 0-1, G: 0-1, B: 0-1)" << std::endl;
  std::cout << "    c .......... CMY-Modus (C: 0-1, M: 0-1, Y: 0-1)" << std::endl;
  std::cout << "    h .......... HSV-Modus (H: 0-360, S: 0-1, V: 0-1)" << std::endl;
  std::cout << std::endl;
  std::cout << "  FARBE EINGEBEN" << std::endl;
  std::cout << "    e .......... Farbwerte eingeben (3x bestaetigen mit Enter)" << std::endl;
  std::cout << std::endl;
  std::cout << "  UMRECHNUNGSMODUS" << std::endl;
  std::cout << "    u .......... Umrechnungsmodus an/aus" << std::endl;
  std::cout << "                 (Werte werden nur umgerechnet," << std::endl;
  std::cout << "                  keine Farbaenderung an Objekten)" << std::endl;
  std::cout << std::endl;
  std::cout << "  SONSTIGES" << std::endl;
  std::cout << "    ESC ........ Programm beenden" << std::endl;
  std::cout << "    m .......... Anleitung erneut drucken" << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << std::endl;
}
/*
 Callback for char input.
 */
std::string inputBuffer = "";
void glutKeyboard(unsigned char keycode, int x, int y)
{

  if (waitingForInput) {
    if (keycode == 13) { // Enter
      if (!inputBuffer.empty()) {
        try {
          float val = std::stof(inputBuffer);
          inputBuffer = "";
          inputValues[inputStep] = val;
          inputStep++;
          std::cout << std::endl;
          if (inputStep == 3) {
            applyInput();
            waitingForInput = false;
            inputStep = 0;
            std::cout << "Farbe gesetzt!" << std::endl;
            glutPostRedisplay();
          } else {
            printInputPrompt();
          }
        } catch (...) {
          inputBuffer = "";
          std::cout << "\nUngültige Eingabe, nochmal: ";
          printInputPrompt();
        }
      }
    }
    else if (keycode == 8) { // Backspace
      if (!inputBuffer.empty()) {
        inputBuffer.pop_back();
        std::cout << "\r";
        printInputPrompt();
        std::cout << inputBuffer << std::flush;
      }
    }
    else if ((keycode >= '0' && keycode <= '9') || keycode == '.' || keycode == '-') {
      inputBuffer += keycode;
      std::cout << keycode << std::flush; // Echo Zeichen
    }
    return;
  }

  switch (keycode) {
    case 27: // ESC
      glutDestroyWindow(glutID);
      return;

      // Farb-Modus wählen
    case 'r': case 'R':
      currentColorMode = ColorMode::RGB;
      std::cout << "Modus: RGB" << std::endl;
      break;
    case 'c': case 'C':
      currentColorMode = ColorMode::CMY;
      std::cout << "Modus: CMY" << std::endl;
      break;
    case 'h': case 'H':
      currentColorMode = ColorMode::HSV;
      std::cout << "Modus: HSV" << std::endl;
      break;

      // Objekt wählen
    case 't': case 'T':
      editingTriangle = true;
      std::cout << "Editiere: Dreieck" << std::endl;
      break;
    case 'q': case 'Q':
      editingTriangle = false;
      std::cout << "Editiere: Quad" << std::endl;
      break;

    case 'e': case 'E':
      waitingForInput = true;
      inputStep       = 0;
      inputBuffer     = "";
      printInputPrompt();
      break;

      // Umrechen Modus
    case 'u': case 'U':
      convertMode = !convertMode;
      std::cout << "Umrechnungsmodus: " << (convertMode ? "AN" : "AUS") << std::endl;
      break;

    case 'm': case 'M':
      printHelp();
      break;
  }
  glutPostRedisplay();
}



int main(int argc, char** argv)
{

  // GLUT: Initialize freeglut library (window toolkit).
  glutInitWindowSize    (WINDOW_WIDTH, WINDOW_HEIGHT);
  glutInitWindowPosition(40,40);
  glutInit(&argc, argv);
  
  // GLUT: Create a window and opengl context (version 4.3 core profile).
  glutInitContextVersion(4, 3);
  glutInitContextFlags  (GLUT_FORWARD_COMPATIBLE | GLUT_DEBUG);
  glutInitDisplayMode   (GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH | GLUT_MULTISAMPLE);
  
  glutCreateWindow("Aufgabenblatt 01");
  glutID = glutGetWindow();
  
  // GLEW: Load opengl extensions
  //glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    return -1;
  }
#if _DEBUG
  if (glDebugMessageCallback) {
    std::cout << "Register OpenGL debug callback " << std::endl;
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(cg::glErrorVerboseCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE,
			  GL_DONT_CARE,
			  GL_DONT_CARE,
			  0,
			  nullptr,
			  true); // get all debug messages
  } else {
    std::cout << "glDebugMessageCallback not available" << std::endl;
  }
#endif

  // GLUT: Set callbacks for events.
  glutReshapeFunc(glutResize);
  glutDisplayFunc(glutDisplay);
  //glutIdleFunc   (glutDisplay); // redisplay when idle
  
  glutKeyboardFunc(glutKeyboard);
  
  // init vertex-array-objects.
  bool result = init();
  if (!result) {
    return -2;
  }
  printHelp();
  // GLUT: Loop until the user closes the window
  // rendering & event handling
  glutMainLoop ();

  // Cleanup in destructors:
  // Objects will be released in ~Object
  // Shader program will be released in ~GLSLProgram
  
  return 0;
}

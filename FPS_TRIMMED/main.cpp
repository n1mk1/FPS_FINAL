#include <GL/freeglut.h>
#include <SOIL.h> // Include SOIL for texture loading
#include <cmath>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <glm/glm.hpp>
#include <time.h>
#define M_PI 3.14

float scaleRobot = 2.0f; // Robot size

// Structs and Global Variables
struct Bullet {
    float x, y, z;
    float dirX, dirY, dirZ;
    bool isPlayerBullet;
};

struct Sphere {
    float x, y, z;
    float radius = 1.0 * scaleRobot; // Radius dependent on the robot's scale
};

// Plane dimensions
const int planeSize = 50;

// Camera position
float cameraX = 0.0f, cameraY = 5.0f, cameraZ = planeSize - 5.0f; // Position at back of room
float cameraAngleH = 0.0f; // Horizontal angle
float cameraAngleV = 0.0f; // Vertical angle (for yaw)

// Jumping mechanics
bool isJumping = false;
float jumpVelocity = 0.2f; // Vertical velocity
const float gravity = -0.003f; // Gravity effect
const float jumpStrength = 0.3f; // Initial jump velocity
const float groundLevel = 5.0f; // Default ground level

// Mouse sensitivity
const float sensitivity = 0.001f;

// Cannon dimensions
const float cannonBaseLength = 5.0f;
const float cannonBaseRadius = 0.1f;

const float cannonBarrelLength = cannonBaseLength * 1.5;
const float cannonBarrelRadius = cannonBaseRadius * 0.5;

// Key state tracking
std::unordered_set<unsigned char> activeKeys;

// Texture IDs
GLuint planeTexture;
GLuint wallTexture;
GLuint uiTexture;
GLuint robotTexture;
GLuint gunTexture;
GLuint cannonTexture;
GLuint beltTexture;

// Bullet container
std::vector<Bullet> bullets;

// Sphere container
std::vector<Sphere> spheres;

//// Mesh importing stuff
// 3D Vector
typedef struct Vector3D
{
    GLdouble x, y, z;
} Vector3D;

// Quads and Vertices of custom mesh
typedef struct Vertex
{
    GLdouble x, y, z;
    Vector3D normal;
    int numQuads;
    int quadIndex[4];
} Vertex;

typedef struct Quad
{
    int vertexIndex[4]; // 4 vertex indices in clockwise order
    Vector3D normal;
} Quad;
bool meshImported = false;

// Array of quads, vertices for mesh
// Note: Arrays follow formats:
//      varray = (Vertex *)malloc(subcurve.numCurvePoints* NUMBEROFSIDES * sizeof(Vertex))
//      qarray = (Quad *)malloc(sizeof(Quad)*(subcurve.numCurvePoints-1)*NUMBEROFSIDES);
// numCurvePoints and NUMBEROFSIDES hard coded from A2,     adjust to match mesh as needed
Quad* qarray = (Quad*)malloc(sizeof(Quad) * (33 - 1) * 16);;

Vertex* varray= (Vertex*)malloc(33 * 16 * sizeof(Vertex));

//// Robot Importing
#define NUM_ROBOTS 2
float robotFireInterval = 2000; // Bullet fire interval in MS
float robotFireActive = false;

typedef struct Position {
    float x = 0.0, y = 0.0, z = 0.0; // Robot position Y is set by spawnRobots() later on
} Position;

typedef struct Robot {
    Position pos;
    bool isActive = false;

    float legAngle = 0.0f;
    float lowerLegAngle = 0.0f;
    float armAngle = 0.0f;
    float lowerArmAngle = 0.0f;
    float bodyLeanAngle = 0.0f;

    bool isWalking = true;
    bool legForward = true;
    bool isSpinning = false;
    float cannonRotation = 0.0f;

    Sphere collisionSphere;

    float speed = 0.05f; // Walking speed

    int health = 3;         // Health of the robot
    float rednessFactor = 0.0f; // Redness level (increases as health decreases)

    bool isHit = false;

    // Used for the robot's defeat animation
    bool isDestroyed = false;
    float upperBodyAngle = 0.0f;
    float headOffsetY = 0.0f;
    float headOffsetZ = 0.0f;
} Robot;


Robot robots[NUM_ROBOTS];

// Cannon collision and disabling
Sphere cannonCollisionSphere = { 0.0f, 0.0f, 0.0f, 2.0f }; // Cannon hitbox
float cannonAngle = 0.0f;
bool isCannonDisabled = false;

// Function Declarations
GLuint loadTexture(const char* fileName);
void drawPlane();
void drawWalls();
void drawBullets();
void drawCannon();
void drawUIOverlay();
void drawSpheres();
void setCamera();
void display();
void handleMovement();
void fireBullet();
void spawnSphere();
void checkCollisions();
void keyboard(unsigned char key, int x, int y);
void keyboardUp(unsigned char key, int x, int y);
void mouseClick(int button, int state, int x, int y);
void mouseMotion(int x, int y);
void reshape(int w, int h);

void drawMeshQuads();
void loadMesh();

void drawRobots();
void spawnRobots();

void drawBot(Robot robot);
void drawHead(Robot robot);
void drawNeck(Robot robot);
void drawBody(Robot robot);
void drawArm(bool isLeft, Robot robot);
void drawLeg(bool isLeft, Robot robot);
void drawCube(float width, float height, float depth);
void drawSolidCube(float size);
void drawSolidSphere(float radius, int slices, int stacks);
void drawCylinder(float radius, float height, int slices);
void drawTrapezoid(float topWidth, float bottomWidth, float height, float depth);

void checkRobotCollisions();
void robotFireHandler(int param);

void checkCannonCollisions();
void disableCannonHandler(int param);
void enableCannonHandler(int param);

void robotHitReset(int robotIndex);
void robotDestroyHandler(int robotIndex);

// Function Definitions

// Function to load a texture
GLuint loadTexture(const char* fileName) {
    GLuint tex;
    tex = SOIL_load_OGL_texture(
        fileName,
        SOIL_LOAD_AUTO,
        SOIL_CREATE_NEW_ID,
        SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y | SOIL_FLAG_TEXTURE_REPEATS
    );

    if (!tex) {
        printf("SOIL loading error: '%s'\n", SOIL_last_result());
    }

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return tex;
}

// Function to draw a textured plane
void drawPlane() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, planeTexture);

    glColor3f(1.0f, 1.0f, 1.0f); // White to show texture colors
    glBegin(GL_QUADS);

    // Draw a single large quad for the plane with the texture mapped across it
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-planeSize, 0.0f, -planeSize);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(planeSize, 0.0f, -planeSize);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(planeSize, 0.0f, planeSize);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-planeSize, 0.0f, planeSize);

    glEnd();

    glDisable(GL_TEXTURE_2D);
}

// Function to draw textured walls
void drawWalls() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, wallTexture);

    glColor3f(1.0f, 1.0f, 1.0f); // White to show texture colors
    glBegin(GL_QUADS);

    // Front wall
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-planeSize, 0.0f, -planeSize);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(planeSize, 0.0f, -planeSize);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(planeSize, planeSize, -planeSize);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-planeSize, planeSize, -planeSize);

    // Back wall
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-planeSize, 0.0f, planeSize);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(planeSize, 0.0f, planeSize);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(planeSize, planeSize, planeSize);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-planeSize, planeSize, planeSize);

    // Left wall
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-planeSize, 0.0f, -planeSize);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-planeSize, 0.0f, planeSize);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-planeSize, planeSize, planeSize);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-planeSize, planeSize, -planeSize);

    // Right wall
    glTexCoord2f(0.0f, 0.0f); glVertex3f(planeSize, 0.0f, -planeSize);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(planeSize, 0.0f, planeSize);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(planeSize, planeSize, planeSize);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(planeSize, planeSize, -planeSize);

    glEnd();

    glDisable(GL_TEXTURE_2D);
}

// Function to draw bullets
void drawBullets() {
    glColor3f(1.0f, 1.0f, 0.0f); // Yellow color for bullets
    for (const Bullet& bullet : bullets) {
        glPushMatrix();
        glTranslatef(bullet.x, bullet.y, bullet.z);
        glutSolidSphere(0.2f, 16, 16); // Draw bullet as a small sphere
        glPopMatrix();
    }
}

// Function to draw spheres
void drawSpheres() {
    glColor3f(1.0f, 0.0f, 0.0f); // Red color for spheres
    for (const Sphere& sphere : spheres) {
        glPushMatrix();
        glTranslatef(sphere.x, sphere.y, sphere.z);
        drawSolidSphere(0.3f, 32, 32); // Draw sphere
        glPopMatrix();
    }
}

void drawCannon() {
    // Set cannon's collision sphere position
    cannonCollisionSphere.x = cameraX;
    cannonCollisionSphere.y = cameraY - 1.5f;
    cannonCollisionSphere.z = cameraZ;

    glPushMatrix();

    // Position the cannon higher on the screen
    glTranslatef(cameraX, cameraY - 0.5f, cameraZ);
    glRotatef(-cameraAngleH * 180.0f / M_PI, 0.0f, 1.0f, 0.0f);
    glRotatef(cameraAngleV * 180.0f / M_PI, 1.0f, 0.0f, 0.0f);
    glTranslatef(0.0f, -0.5f, -2.5f);

    // Rotate cannon (will rotate downward if disabled)
    glRotatef(cannonAngle, 1.0f, 0.0f, 0.0f);

    // Draw base of the cannon (with texture)
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gunTexture); // Bind 'rough.png'

    GLUquadric* quadric = gluNewQuadric();
    gluQuadricTexture(quadric, GL_TRUE);

    glColor3f(1.0f, 1.0f, 1.0f); // White to display the texture properly
    gluCylinder(quadric, 0.2f, 0.2f, 2.0f, 32, 32);
    glDisable(GL_TEXTURE_2D); // Disable texture for other parts

    // Draw the cannon barrel (with muzzle texture)
    glTranslatef(0.0f, 0.0f, -2.0f);  // Move to position for the barrel
    glBindTexture(GL_TEXTURE_2D, cannonTexture);  // Bind muzzle texture
    glEnable(GL_TEXTURE_2D);  // Enable texturing

    glColor3f(1.0f, 1.0f, 1.0f); // White to properly display the texture
    gluCylinder(quadric, 0.1f, 0.1f, 3.0f, 32, 32);  // Draw barrel with the new texture

    glDisable(GL_TEXTURE_2D); // Disable texture after drawing barrel

    // Draw the scope for the cannon
    glPushMatrix();
    glTranslatef(0.0f, 0.2f, 1.0f); // Position the scope above the barrel
    glColor3f(0.3f, 0.3f, 0.3f); // Darker grey color for the scope

    // Draw the scope cylinder
    gluCylinder(quadric, 0.05f, 0.05f, 1.0f, 32, 32); // Increase slices for smoother look

    // Draw the scope lens (front)
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 1.0f);
    glColor3f(0.1f, 0.1f, 0.1f); // Dark color for the lens
    gluDisk(quadric, 0.0f, 0.05f, 32, 1); // Draw a disk at the end of the scope with higher slices
    glPopMatrix();

    // Draw the back lens of the scope
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -0.05f); // Slightly behind the start of the scope
    glColor3f(0.1f, 0.1f, 0.1f); // Same color for the back lens
    gluDisk(quadric, 0.0f, 0.05f, 32, 1); // Draw a disk at the back of the scope
    glPopMatrix();

    // Add details to make the scope more realistic
    glPushMatrix();
    glTranslatef(0.0f, 0.08f, 0.5f); // Position the adjustment knob on top of the scope
    glColor3f(0.2f, 0.2f, 0.2f); // Darker grey for the knob
    gluCylinder(quadric, 0.02f, 0.02f, 0.1f, 16, 16); // Draw the adjustment knob
    glTranslatef(0.0f, 0.0f, 0.1f);
    gluDisk(quadric, 0.0f, 0.02f, 16, 1); // Cap the knob with a disk
    glPopMatrix();

    glPopMatrix();

    // Render the imported mesh with robotTexture
    if (!meshImported) {
        loadMesh(); // Ensure the mesh is loaded
        meshImported = true;
    }

    glPushMatrix();
    // Position and scale the mesh appropriately relative to the cannon
    glTranslatef(0.0f, -3.0f, 1.0f); // Adjust position as needed
    glScalef(0.51f, 0.51f, 0.51f);  // Adjust scale as needed

    // Enable texturing and bind the robotTexture
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, beltTexture); // Bind the robot texture


    glColor3f(1.0f, 1.0f, 1.0f); // White to properly display the texture
    drawMeshQuads(); // Render the imported mesh with the new texture

    // Disable texturing after rendering
    glDisable(GL_TEXTURE_2D);
    glPopMatrix();

    // Cleanup
    gluDeleteQuadric(quadric);

    glPopMatrix();
}



void drawRobots() {
    for (int i = 0; i < NUM_ROBOTS; i++) {
        if (robots[i].isActive) {
            glPushMatrix();
                // Place robot in the room
                glTranslatef(robots[i].pos.x, robots[i].pos.y, robots[i].pos.z);

                // Make the robot face the camera
                float dirX = cameraX - robots[i].pos.x;
                float dirZ = cameraZ - robots[i].pos.z;
                float angle = atan2(dirX, dirZ) * 180.0f / M_PI;
                glRotatef(angle, 0.0f, 1.0f, 0.0f);

                drawBot(robots[i]);

            glPopMatrix();
            
        }

        // Create "red flash" briefly when robot is hit
        // Draws flash independently of the robot being active so it persists after it dies
        if (robots[i].isHit) {

            glPushMatrix();
            glColor3f(1.0f, 0.0f, 0.0f); // Red color for spheres

            glTranslatef(robots[i].collisionSphere.x, robots[i].collisionSphere.y, robots[i].collisionSphere.z);
            drawSolidSphere(robots[i].collisionSphere.radius, 32, 32); // Draw sphere
            glPopMatrix();
        }
    }
}


void drawBot(Robot robot) {
    glEnable(GL_TEXTURE_2D); // Enable texturing
    glBindTexture(GL_TEXTURE_2D, robotTexture); // Bind robot texture

    //glEnable(GL_BLEND); // Get transparency for texture
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glScalef(scaleRobot, scaleRobot, scaleRobot);

    // Adjust colors based on rednessFactor
    float baseRed = 1.0f, baseGreen = 0.55f, baseBlue = 0.0f;
    float red = baseRed + robot.rednessFactor;
    float green = baseGreen - robot.rednessFactor * 0.2f; // Slightly desaturate green
    float blue = baseBlue - robot.rednessFactor * 0.2f;  // Slightly desaturate blue

    // Clamp the colors to avoid overflow
    red = std::min(red, 1.0f);
    green = std::max(green, 0.0f);
    blue = std::max(blue, 0.0f);

    glColor3f(red, green, blue);

    glPushMatrix();
    // Rotate body forwards when defeated
    glTranslatef(0.0, -0.4 * scaleRobot, 0.0);
    glRotatef(robot.upperBodyAngle, 1.0, 0.0, 0.0);
    glTranslatef(0.0, 0.4 * scaleRobot, 0.0);

    glPushMatrix();
    // Make head fall off when defeated
    glTranslatef(0.0, robot.headOffsetY, robot.headOffsetZ);
    drawHead(robot);
    glPopMatrix();

    drawNeck(robot);
    drawBody(robot);
    drawArm(true, robot);
    drawArm(false, robot);
    glPopMatrix();
    drawLeg(true, robot);
    drawLeg(false, robot);

    //glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D); // Disable texturing after
}


void drawHead(Robot robot) {
    // Draw Head - Bottom part (Cube)
    glPushMatrix();
        glColor3f(1.0f, 0.55f, 0.0f);
        glTranslatef(0.0f, 1.3f, 0.0f);
        glScalef(1.05f, 1.0f, 1.05f);
        drawCube(1.0f, 1.0f, 1.0f);
    glPopMatrix();

    // Draw Head - Top part (Sphere)
    glPushMatrix();
        glColor3f(1.0f, 0.6f, 0.0f);
        glTranslatef(0.0f, 1.85f, 0.0f);
        drawSolidSphere(0.6f, 20, 20);
    glPopMatrix();

    // Draw Ears + Antennas
    for (int i = -1; i <= 1; i += 2) {
        // Ears
        glPushMatrix();
            glColor3f(0.9f, 0.45f, 0.0f);
            glTranslatef(i * 0.6f, 1.5f, 0.0f);
            glScalef(0.2f, 0.3f, 0.05f);
            drawCube(0.5f, 1.0f, 4.0f);
        glPopMatrix();

        // Antennas
        glPushMatrix();
            glColor3f(0.7f, 0.3f, 0.0f);
            glTranslatef(i * 0.6f, 2.3f, -0.5f);
            glRotatef(60, 1.0f, 0.0f, 0.0f);
            drawCylinder(0.05f, 1.0f, 10);
        glPopMatrix();
    }

    // Draw Screen and Visor
    glPushMatrix();
        glColor3f(0.0f, 0.0f, 0.0f);
        glTranslatef(0.0f, 1.4f, 0.54f);
        glScalef(0.8f, 0.5f, 0.05f);
        drawCube(1.0f, 1.0f, 1.0f);
    glPopMatrix();

    glPushMatrix();
        glColor3f(1.0f, 0.5f, 0.0f);
        glTranslatef(0.0f, 1.8f, 0.8f);
        glScalef(1.2f, 0.1f, 0.5f);
        drawCube(1.0f, 1.0f, 1.0f);
    glPopMatrix();
}

void drawNeck(Robot robot) {
    glPushMatrix();
        glColor3f(0.0f, 0.0f, 0.0f);
        glTranslatef(0.0f, 0.95f, 0.0f);
        glRotatef(90, 1.0f, 0.0f, 0.0f);
        drawCylinder(0.2f, 0.3f, 20);
    glPopMatrix();
}

void drawBody(Robot robot) {
    // leaning 
    glPushMatrix();
        glRotatef(robot.bodyLeanAngle, 0.0f, 0.0f, 1.0f);

        // Draw Body and jetpack
        glPushMatrix();
            glColor3f(1.0f, 0.55f, 0.0f);
            glTranslatef(0.0f, 0.2f, 0.0f);
            drawTrapezoid(1.2f, 1.0f, 1.0f, 0.5f);
        glPopMatrix();

        glPushMatrix();
            glColor3f(0.8f, 0.45f, 0.0f);
            glTranslatef(0.0f, 0.3f, -0.4f);
            glScalef(0.7f, 0.9f, 0.3f);
            drawCube(1.0f, 1.0f, 1.0f);
        glPopMatrix();

        // Draw Black Ridge and Hips
        glPushMatrix();
            glColor3f(0.6f, 0.3f, 0.0f);
            glTranslatef(0.0f, -0.35f, 0.0f);
            drawCube(1.0f, 0.1f, 0.5f);
        glPopMatrix();

        glPushMatrix();
            glColor3f(0.9f, 0.5f, 0.1f);
            glTranslatef(0.0f, -0.6f, 0.0f);
            drawTrapezoid(1.0f, 0.7f, 0.4f, 0.5f);
        glPopMatrix();

    glPopMatrix();
}

// Note: Right arm will be drawn facing outward
void drawArm(bool isLeft, Robot robot) {
    float direction = isLeft ? -1.0f : 1.0f;
    float armRotation = isLeft ? -90 : -robot.armAngle;
    float lowerArmRotation = isLeft ? robot.lowerArmAngle : -robot.lowerArmAngle;

    // Connect Shoulder + Upper Arm
    glPushMatrix();
        glColor3f(0.0f, 0.0f, 0.0f);
        glTranslatef(0.85f * direction, 0.9f, 0.0f);
        drawSolidSphere(0.25, 20, 20);
    glPopMatrix();

    glPushMatrix();
        glColor3f(1.0f, 0.55f, 0.0f);
        glTranslatef(0.85f * direction, 0.65f, 0.0f);
        glRotatef(armRotation, 1.0f, 0.0f, 0.0f);
        glTranslatef(0.0f, -0.25f, 0.0f);
        drawCube(0.3f, 0.5f, 0.3f);

        // Connect Elbow + Lower Arm
        glPushMatrix();
            glColor3f(0.8f, 0.3f, 0.0f);
            glTranslatef(0.0f, -0.4f, 0.0f);
            drawSolidSphere(0.15, 20, 20);
        glPopMatrix();

        glPushMatrix();
            glColor3f(1.0f, 0.4f, 0.0f);
            glTranslatef(0.0f, -0.7f, 0.0f);
            glRotatef(lowerArmRotation, 1.0f, 0.0f, 0.0f);
            glTranslatef(0.0f, -0.1f, 0.0f);
            drawCube(0.3f, 0.5f, 0.3f);

            // Cannon
            glPushMatrix();
                glColor3f(0.8f, 0.3f, 0.0f);
                glTranslatef(0.0f, -0.0f, 0.0f);
                if (robot.isSpinning) glRotatef(robot.cannonRotation, 0.0f, 1.0f, 0.0f);
                glRotatef(90, 1.0f, 0.0f, 0.0f);
                drawCylinder(0.2f, 0.5f, 20);

                // Spinning Indicators for cannon
                if (robot.isSpinning) {
                    for (int i = -1; i <= 1; i += 2) {
                        glPushMatrix();
                            glColor3f(1.0f, 0.6f, 0.2f);
                            glTranslatef(i * 0.4f, 0.0f, 0.1f);
                            glRotatef(robot.cannonRotation, 0.0f, 1.0f, 0.0f);
                            drawCube(0.05f, 0.05f, 0.05f);
                        glPopMatrix();
                    }
                    glPushMatrix();
                        glColor3f(1.0f, 0.8f, 0.4f);
                        glTranslatef(0.0f, 0.5f, 0.0f);
                        glRotatef(robot.cannonRotation, 0.0f, 1.0f, 0.0f);
                        glScalef(1.2f, 1.2f, 1.2f);
                        drawSolidSphere(0.05f, 20, 20);
                    glPopMatrix();
                }
            glPopMatrix();
        glPopMatrix();
    glPopMatrix();
}

void drawLeg(bool isLeft, Robot robot) {
    float direction = isLeft ? -1.0f : 1.0f;
    float legRotation = isLeft ? robot.legAngle : -robot.legAngle;
    float lowerLegRotation = isLeft ? robot.lowerLegAngle : -robot.lowerLegAngle;

    // Draw Hip, Upper Leg, Knee, and Lower Leg
    glPushMatrix();
        glColor3f(0.0f, 0.0f, 0.0f);
        glTranslatef(0.4f * direction, -0.75f, 0.0f);
        drawSolidSphere(0.25, 20, 20);
    glPopMatrix();

    glPushMatrix();
        glColor3f(1.0f, 0.55f, 0.0f);
        glTranslatef(0.4f * direction, -1.125f, 0.0f);
        glRotatef(legRotation, 1.0f, 0.0f, 0.0f);
        glTranslatef(0.0f, -0.375f, 0.0f);
        drawCube(0.4f, 0.75f, 0.4f);

        glPushMatrix();
            glColor3f(0.8f, 0.3f, 0.0f);
            glTranslatef(0.0f, -0.375f, 0.0f);
            drawSolidSphere(0.25, 20, 20);
        glPopMatrix();

        glPushMatrix();
            glColor3f(1.0f, 0.4f, 0.0f);
            glTranslatef(0.0f, -0.75f, 0.0f);
            glRotatef(lowerLegRotation, 1.0f, 0.0f, 0.0f);
            glTranslatef(0.0f, -0.25f, 0.0f);
            drawCube(0.4f, 0.75f, 0.4f);
        glPopMatrix();

    glPopMatrix();
}

// Draws solid cube w/ dimensions given
void drawCube(float width, float height, float depth) {
    glPushMatrix();
        glScalef(width, height, depth);
        drawSolidCube(1.0);
    glPopMatrix();
}

void drawCylinder(float radius, float height, int slices) {
    GLUquadric* quadric = gluNewQuadric();

    gluQuadricTexture(quadric, GL_TRUE); // Auto map texture
    gluCylinder(quadric, radius, radius, height, slices, 1);

    // Bottom cap of cylinder
    glPushMatrix();
        glTranslatef(0.0f, 0.0f, 0.0f);
        gluDisk(quadric, 0.0f, radius, slices, 1);
    glPopMatrix();

    // Top cap
    glPushMatrix();
        glTranslatef(0.0f, 0.0f, height);
        gluDisk(quadric, 0.0f, radius, slices, 1);
    glPopMatrix();

    gluDeleteQuadric(quadric);
}

void drawTrapezoid(float topWidth, float bottomWidth, float height, float depth) {
    float halfTopWidth = topWidth / 2.0f;
    float halfBottomWidth = bottomWidth / 2.0f;
    float halfDepth = depth / 2.0f;

    glBegin(GL_QUADS);
        // Front face
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-halfTopWidth, height / 2.0f, halfDepth);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(halfTopWidth, height / 2.0f, halfDepth);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(halfBottomWidth, -height / 2.0f, halfDepth);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-halfBottomWidth, -height / 2.0f, halfDepth);

        // Back face
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-halfTopWidth, height / 2.0f, -halfDepth);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(halfTopWidth, height / 2.0f, -halfDepth);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(halfBottomWidth, -height / 2.0f, -halfDepth);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-halfBottomWidth, -height / 2.0f, -halfDepth);

        // Left face
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-halfTopWidth, height / 2.0f, halfDepth);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(-halfTopWidth, height / 2.0f, -halfDepth);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(-halfBottomWidth, -height / 2.0f, -halfDepth);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-halfBottomWidth, -height / 2.0f, halfDepth);

        // Right face
        glTexCoord2f(0.0f, 0.0f); glVertex3f(halfTopWidth, height / 2.0f, halfDepth);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(halfTopWidth, height / 2.0f, -halfDepth);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(halfBottomWidth, -height / 2.0f, -halfDepth);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(halfBottomWidth, -height / 2.0f, halfDepth);

        // Top face
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-halfTopWidth, height / 2.0f, halfDepth);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(halfTopWidth, height / 2.0f, halfDepth);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(halfTopWidth, height / 2.0f, -halfDepth);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-halfTopWidth, height / 2.0f, -halfDepth);

        // Bottom face
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-halfBottomWidth, -height / 2.0f, halfDepth);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(halfBottomWidth, -height / 2.0f, halfDepth);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(halfBottomWidth, -height / 2.0f, -halfDepth);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-halfBottomWidth, -height / 2.0f, -halfDepth);
    glEnd();
}

// Draws a solid cube with proper texture mapping
void drawSolidCube(float size) {
    float halfSize = size / 2.0f;

    glBegin(GL_QUADS);
        // Front face
        glNormal3f(0.0f, 0.0f, 1.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-halfSize, -halfSize, halfSize);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(halfSize, -halfSize, halfSize);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(halfSize, halfSize, halfSize);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-halfSize, halfSize, halfSize);

        // Back face
        glNormal3f(0.0f, 0.0f, -1.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(halfSize, -halfSize, -halfSize);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(-halfSize, -halfSize, -halfSize);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(-halfSize, halfSize, -halfSize);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(halfSize, halfSize, -halfSize);

        // Left face
        glNormal3f(-1.0f, 0.0f, 0.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-halfSize, -halfSize, -halfSize);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(-halfSize, -halfSize, halfSize);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(-halfSize, halfSize, halfSize);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-halfSize, halfSize, -halfSize);

        // Right face
        glNormal3f(1.0f, 0.0f, 0.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(halfSize, -halfSize, halfSize);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(halfSize, -halfSize, -halfSize);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(halfSize, halfSize, -halfSize);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(halfSize, halfSize, halfSize);

        // Top face
        glNormal3f(0.0f, 1.0f, 0.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-halfSize, halfSize, halfSize);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(halfSize, halfSize, halfSize);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(halfSize, halfSize, -halfSize);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-halfSize, halfSize, -halfSize);

        // Bottom face
        glNormal3f(0.0f, -1.0f, 0.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-halfSize, -halfSize, -halfSize);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(halfSize, -halfSize, -halfSize);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(halfSize, -halfSize, halfSize);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-halfSize, -halfSize, halfSize);
    glEnd();
}


// Draw spheres using GLU instead of using GLUT primitives
void drawSolidSphere(float radius, int slices, int stacks) {
    GLUquadric* quadric = gluNewQuadric();
    gluQuadricDrawStyle(quadric, GLU_FILL); // Draw sphere as solid shape
    gluQuadricNormals(quadric, GLU_SMOOTH); // Have smooth normals for lighting

    gluQuadricTexture(quadric, GL_TRUE); // Automatic texture generation

    // Draw sphere
    gluSphere(quadric, radius, slices, stacks);

    gluDeleteQuadric(quadric);
}

void moveRobotTowardsCamera(Robot& robot) {
    if (!robot.isActive || robot.isDestroyed) return;

    static float stepProgress = 0.0f; // Tracks the progress of the current step
    const float stepFrequency = 0.005f; // Slower frequency for deliberate steps
    const float stepHeight = 0.8f; // Increased vertical lift for stomping
    const float bodyTiltAngle = 5.0f; // Angle to tilt the body toward the support leg
    const float stopDuration = 0.2f; // Pause duration between steps

    static bool isStopping = false;
    static float stopTimer = 0.0f;

    // Calculate direction vector
    float dirX = cameraX - robot.pos.x;
    float dirY = cameraY - robot.pos.y;
    float dirZ = cameraZ - robot.pos.z;
    float length = sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);

    // Normalize the direction vector
    dirX /= length;
    dirY /= length;
    dirZ /= length;

    // Apply zigzag motion perpendicular to the forward direction
    static float zigzagAngle = 0.0f;
    const float zigzagFrequency = 0.01f;
    const float zigzagAmplitude = 0.3f;
    float zigzagOffsetX = -dirZ * zigzagAmplitude * sin(zigzagAngle);
    float zigzagOffsetZ = dirX * zigzagAmplitude * sin(zigzagAngle);

    // Update zigzag angle
    zigzagAngle += zigzagFrequency;

    // Stop between steps
    if (isStopping) {
        stopTimer += stepFrequency;
        if (stopTimer >= stopDuration) {
            isStopping = false;
            stopTimer = 0.0f;
        }
        return; // Do not proceed with movement while stopping
    }

    // Update robot position
    robot.pos.x += (dirX + zigzagOffsetX) * robot.speed * 0.3f; // Slower forward movement
    robot.pos.z += (dirZ + zigzagOffsetZ) * robot.speed * 0.3f;

    // Update collision sphere position
    robot.collisionSphere.x = robot.pos.x;
    robot.collisionSphere.z = robot.pos.z;

    // Step progression and animation
    stepProgress += stepFrequency;

    if (stepProgress >= 1.0f) {
        stepProgress = 0.0f;
        robot.legForward = !robot.legForward; // Switch legs
        isStopping = true; // Pause for dramatic effect
    }

    float stepLift = sin(stepProgress * M_PI) * stepHeight; // Sinusoidal lift motion

    // Lift and drop one leg while keeping the other leg as support
    if (robot.legForward) {
        // Left leg stepping
        robot.legAngle = stepLift * 30.0f; // Exaggerated forward lift
        robot.lowerLegAngle = -robot.legAngle * 0.8f;

        // Raise the knee sphere
        robot.pos.y = groundLevel + stepLift;

        // Rotate the quadriceps to mimic a stomp
        robot.lowerLegAngle += stepLift * 20.0f;

        // Tilt body toward the right leg (support leg)
        robot.bodyLeanAngle = -bodyTiltAngle;
    }
    else {
        // Right leg stepping
        robot.legAngle = -stepLift * 30.0f;
        robot.lowerLegAngle = -robot.legAngle * 0.8f;

        // Raise the knee sphere
        robot.pos.y = groundLevel + stepLift;

        // Rotate the quadriceps to mimic a stomp
        robot.lowerLegAngle -= stepLift * 20.0f;

        // Tilt body toward the left leg (support leg)
        robot.bodyLeanAngle = bodyTiltAngle;
    }

    // Simulate a slight body tilt when transitioning between steps
    if (stepProgress > 0.8f) {
        robot.bodyLeanAngle *= 0.5f; // Reduce tilt as the robot transitions to the next step
    }
}


// Function to draw the UI overlay (crosshair)
void drawUIOverlay() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
        glLoadIdentity();
        gluOrtho2D(0, 1920, 0, 1080); // Set orthographic projection matching window size

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
            glLoadIdentity();

            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, uiTexture);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Enable transparency handling

            glColor3f(1.0f, 1.0f, 1.0f); // Set color to white to display texture properly

            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex2f(910.0f, 475.0f);    // Bottom-left corner (lowered by 50 units)
            glTexCoord2f(1.0f, 0.0f); glVertex2f(1010.0f, 475.0f);   // Bottom-right corner (lowered by 50 units)
            glTexCoord2f(1.0f, 1.0f); glVertex2f(1010.0f, 575.0f);   // Top-right corner (lowered by 50 units)
            glTexCoord2f(0.0f, 1.0f); glVertex2f(910.0f, 575.0f);    // Top-left corner (lowered by 50 units)

            glEnd();

            glDisable(GL_BLEND);
            glDisable(GL_TEXTURE_2D);

            glMatrixMode(GL_PROJECTION);
        glPopMatrix();

        glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// Function to set the camera
void setCamera() {
    float dirX = sin(cameraAngleH) * cos(cameraAngleV);
    float dirY = sin(cameraAngleV);
    float dirZ = -cos(cameraAngleH) * cos(cameraAngleV);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(cameraX, cameraY, cameraZ, cameraX + dirX, cameraY + dirY, cameraZ + dirZ, 0.0f, 1.0f, 0.0f);
}

// Display callback
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    setCamera();

    drawPlane();
    drawWalls();
    drawCannon();
    drawBullets();
    drawSpheres();;

    // Draw active robots
    drawRobots();

    // Render the 2D UI Overlay
    drawUIOverlay();


    glutSwapBuffers();
}

void handleMovement() {
    const float baseSpeed = 0.15f;
    float speed = baseSpeed;

    if (activeKeys.count('c')) {
        speed *= 2.0f;
    }

    float dirX = sin(cameraAngleH) * cos(cameraAngleV);
    float dirZ = -cos(cameraAngleH) * cos(cameraAngleV);

    if (activeKeys.count('w')) {
        cameraX += dirX * speed;
        cameraZ += dirZ * speed;
    }
    if (activeKeys.count('s')) {
        cameraX -= dirX * speed;
        cameraZ -= dirZ * speed;
    }
    if (activeKeys.count('a')) {
        cameraX -= cos(cameraAngleH) * speed;
        cameraZ -= sin(cameraAngleH) * speed;
    }
    if (activeKeys.count('d')) {
        cameraX += cos(cameraAngleH) * speed;
        cameraZ += sin(cameraAngleH) * speed;
    }

    if (cameraX < -planeSize + 2.0f) cameraX = -planeSize + 2.0f;
    if (cameraX > planeSize - 2.0f) cameraX = planeSize - 2.0f;
    if (cameraZ < -planeSize + 2.0f) cameraZ = -planeSize + 2.0f;
    if (cameraZ > planeSize - 2.0f) cameraZ = planeSize - 2.0f;

    if (isJumping) {
        cameraY += jumpVelocity;
        jumpVelocity += gravity;

        if (cameraY <= groundLevel) {
            cameraY = groundLevel;
            isJumping = false;
            jumpVelocity = 0.0f;
        }
    }

    for (Robot& robot : robots) {
        moveRobotTowardsCamera(robot);
    }

    for (Bullet& bullet : bullets) {
        bullet.x += bullet.dirX * 0.5f;
        bullet.y += bullet.dirY * 0.5f;
        bullet.z += bullet.dirZ * 0.5f;
    }

    for (Sphere& sphere : spheres) {
        float dirX = cameraX - sphere.x;
        float dirY = cameraY - sphere.y;
        float dirZ = cameraZ - sphere.z;
        float length = sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);

        dirX /= length;
        dirY /= length;
        dirZ /= length;

        sphere.x += dirX * 0.05f;
        sphere.y += dirY * 0.05f;
        sphere.z += dirZ * 0.05f;
    }

    for (Robot& robot : robots) {
        if (robot.isSpinning) {
            robot.cannonRotation += 5.0f;
            if (robot.cannonRotation > 360.0f) robot.cannonRotation -= 360.0f;
        }
    }

    checkCollisions();
    checkRobotCollisions();
    checkCannonCollisions();

    glutPostRedisplay();
}


// Function to fire a bullet
void fireBullet() {
    // Bullet travel direction
    float dirX = sin(cameraAngleH) * cos(cameraAngleV);
    float dirY = sin(cameraAngleV);
    float dirZ = -cos(cameraAngleH) * cos(cameraAngleV);
    
    // Bullet initial spawn position (tip of cannon)
    float tipX = cameraX  + dirX * cannonBaseLength;
    float tipY = (cameraY - 1.0f) + dirY * cannonBaseLength; // Note: (cameraY - 1.0f) is kinda hard coded in here, if you change the cannon position change this too
    float tipZ = cameraZ + dirZ * cannonBaseLength;

    Bullet bullet = { tipX, tipY, tipZ, dirX, dirY, dirZ, true };
    bullets.push_back(bullet);
}


// Allows robots to fire bullets at a set interval
void robotFireHandler(int param) {
    int randomRange = 5; // Max range of direction variation

    // Iterate through all robots, have all active robots fire a bullet
    for (Robot& robot : robots) {
        if (robot.isActive && !robot.isDestroyed) {
            // Note: Direction is calculated from where the end of the arm is, adjust initial offset to match
            // (Basically just hard coded these values)
            float offsetX = -(0.45f * scaleRobot);
            float offsetY = (0.4f * scaleRobot);
            float offsetZ = (1.6f * scaleRobot);

            // Get normalized direction vectors
            float dirX = (cameraX)-(robot.pos.x + offsetX);
            float dirY = (cameraY - 1.5f) - (robot.pos.y + offsetY); // Note: the -1.5f is necessary to shoot at the cannon specifically
            float dirZ = (cameraZ)-(robot.pos.z + offsetZ);

            // Random bullet direction variation
            dirX += (float)(rand() % (2 * randomRange) - randomRange);
            dirY += (float)(rand() % (2 * randomRange) - randomRange);
            dirZ += (float)(rand() % (2 * randomRange) - randomRange);

            float length = sqrt(pow(dirX, 2) + pow(dirY, 2) + pow(dirZ, 2));

            dirX /= length;
            dirY /= length;
            dirZ /= length;

            // Bullet spawn location
            // Adjust offset to match robot arm's end
            float tipX = robot.pos.x + offsetX;
            float tipY = robot.pos.y + offsetY;
            float tipZ = robot.pos.z + offsetZ;

            // Add robot's bullet
            Bullet newBullet = { tipX, tipY, tipZ,
                                dirX, dirY, dirZ,
                                false};

            bullets.push_back(newBullet);
        }
    }

    // Set time for when robot shoots next (in ms)
    glutTimerFunc(robotFireInterval, robotFireHandler, 0);
}

void disableCannonHandler(int param) {
    if (isCannonDisabled && cannonAngle > -10.0f) {
        cannonAngle -= 0.1f; // Move cannon downward
        glutTimerFunc(10, disableCannonHandler, 0);
    }
    else {
        // Re-enable cannon after a second
        glutTimerFunc(2000, enableCannonHandler, 0);
    }
}

void enableCannonHandler(int param) {
    // Play animation, then re-enable firing after
    if (cannonAngle < 0.0f) {
        cannonAngle += 0.2f; // Move cannon upward
        glutTimerFunc(10, enableCannonHandler, 0);
    }
    else {
        isCannonDisabled = false;
    }
}

void robotHitReset(int robotIndex) {
    if (robotIndex >= 0) {
        robots[robotIndex].isHit = false;
    }
}

void robotDeactivate(int robotIndex) {
    if (robotIndex >= 0 && robots[robotIndex].isDestroyed) {
        robots[robotIndex].isActive = false;
    }
}

// Animation that plays when a robot is destroyed
void robotDestroyHandler(int robotIndex) {
    // Animation phase 1: Lean robot forward
    if (robots[robotIndex].isDestroyed && robots[robotIndex].upperBodyAngle < 45.0f) {
        robots[robotIndex].isWalking = false;

        robots[robotIndex].upperBodyAngle += 0.5f;
        glutTimerFunc(10, robotDestroyHandler, robotIndex);
    }
    // Animation phase 2: Move robot head (Head falls off)
    else if (robots[robotIndex].isDestroyed && robots[robotIndex].headOffsetY > -2.2 && robots[robotIndex].headOffsetZ < 2.2) {
        robots[robotIndex].headOffsetY -= 0.05f;
        robots[robotIndex].headOffsetZ += 0.05f;

        glutTimerFunc(10, robotDestroyHandler, robotIndex);
    }
    // Animation phase 3: Pause animation, then deactive robot after a second
    else {
        glutTimerFunc(1000, robotDeactivate, robotIndex);
    }
}


// Function to spawn a sphere
void spawnSphere() {
    // Calculate initial spawn position of spheres 

    float initSphereX = (float)(rand() % (2 * planeSize) - planeSize); // Random X coord along room width
    float initSphereY = 5.0f; // Adjust this later based on robot height / center
    float initSphereZ = ( -planeSize + 1) - (float)(rand() % 3);

    Sphere sphere = { initSphereX, initSphereY, initSphereZ };
    spheres.push_back(sphere);
}


void checkCollisions() {
    const float collisionThreshold = 0.40f; // Collision threshold (radius)
    const float collisionThresholdSquared = collisionThreshold * collisionThreshold; // Precompute squared threshold

    bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
        [&collisionThresholdSquared](Bullet& bullet) { // Capture by reference
            bool bulletRemoved = false;

            for (auto it = spheres.begin(); it != spheres.end();) {
                float dx = bullet.x - it->x;
                float dy = bullet.y - it->y;
                float dz = bullet.z - it->z;
                float distanceSquared = dx * dx + dy * dy + dz * dz;

                if (distanceSquared < collisionThresholdSquared) {
                    it = spheres.erase(it); // Remove sphere
                    bulletRemoved = true;
                    break; // Bullet can't collide with multiple spheres
                }
                else {
                    ++it;
                }
            }
            return bulletRemoved; // Remove bullet if it hit a sphere
        }
    ), bullets.end());
}

void checkRobotCollisions() {
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](const Bullet& bullet) {
        Robot* hitRobot = nullptr;

        // Check collision only if bullet is owned by the PLAYER
        if (!bullet.isPlayerBullet) {
            return false;
        }

        for (Robot* robot = robots; robot < robots + NUM_ROBOTS; ++robot) {
            if (robot->isActive && !robot->isDestroyed) {
                // Calculate distance from bullet to robot's collision sphere center
                float dx = bullet.x - robot->pos.x;
                float dy = bullet.y - robot->pos.y;
                float dz = bullet.z - robot->pos.z;
                float distanceSquared = dx * dx + dy * dy + dz * dz;

                if (distanceSquared < robot->collisionSphere.radius * robot->collisionSphere.radius) {
                    hitRobot = robot;
                    break;
                }
            }
        }

        if (hitRobot) {
            // Reduce robot health and increase redness
            hitRobot->health--;
            hitRobot->rednessFactor += 0.3f;

            int robotIndex = hitRobot - robots; // Get index of robot within robots array
            hitRobot->isHit = true; // Set boolean that will briefly draw a red sphere on hit

            // Reset isHit variable to false after a brief moment
            glutTimerFunc(50, robotHitReset, robotIndex);

            // Deactivate robot if health reaches zero
            if (hitRobot->health <= 0) {
                //hitRobot->isActive = false;
                hitRobot->isDestroyed = true;
                robotDestroyHandler(robotIndex);
            }
            return true; // Remove the bullet
        }
        return false; // Keep the bullet
        }), bullets.end());
}

void checkCannonCollisions() {
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](const Bullet& bullet) {
        // Check collision only if bullet is owned by a robot
        if (bullet.isPlayerBullet) {
            return false;
        }

        // Calculate distance from bullet to cannon's collision sphere
        float dx = bullet.x - cannonCollisionSphere.x;
        float dy = bullet.y - cannonCollisionSphere.y;
        float dz = bullet.z - cannonCollisionSphere.z;
        float distanceSquared = dx * dx + dy * dy + dz * dz;

        // If bullet has collided w/ cannon
        if (distanceSquared < cannonCollisionSphere.radius * cannonCollisionSphere.radius) {
            // Disable cannon when hit
            if (!isCannonDisabled) {
                printf("Cannon has been hit!\n");
                isCannonDisabled = true;
                glutTimerFunc(10, disableCannonHandler, 0); // Play animation
            }
            

            return true; // Remove the bullet (regardless if cannon is disabled or not)
        }
        return false;
        }), bullets.end());
}

void spawnRobots() {
    for (int i = 0; i < NUM_ROBOTS; i++) {
        robots[i].pos.x = (float)(rand() % (2 * planeSize) - planeSize);
        robots[i].pos.y = 6.0f;
        robots[i].pos.z = (-planeSize + 4) - (float)(rand() % 3);
        robots[i].isActive = true;
        robots[i].isWalking = true;
        robots[i].isDestroyed = false;

        robots[i].collisionSphere.x = robots[i].pos.x;
        robots[i].collisionSphere.y = robots[i].pos.y;
        robots[i].collisionSphere.z = robots[i].pos.z;

        robots[i].health = 3; // Reset health
        robots[i].rednessFactor = 0.0f; // Reset redness

        robots[i].upperBodyAngle = 0.0f; // Reset body angle
        robots[i].headOffsetY = 0.0f;
        robots[i].headOffsetZ = 0.0f;
    }

    // Activates timer once to prevent stacking
    if (!robotFireActive) {
        glutTimerFunc(robotFireInterval, robotFireHandler, 0); // Get robots to fire bullets at interval
        robotFireActive = true;
    }

}


// Key press callback
void keyboard(unsigned char key, int x, int y) {
    activeKeys.insert(key);

    if (key == 'q' || key == 'Q' || key == 27) { //  Exit program with q, Q, Esc
        exit(0);
    }
    /*
    if (key == ' ' && !isJumping) { // Jump on space key if not already jumping
        isJumping = true;
        jumpVelocity = jumpStrength;
    }
    */
    if (key == 'f' || key == ' ') { // Fire bullet when 'f' or spacebar key is pressed
        fireBullet();
    }
    /*
    if (key == 'g') { // Spawn sphere when 'g' key is pressed
        spawnSphere();
    }
    */
    if (key == 'e') { // Spawn robot (for testing, put this stuff in key == g later (or comment it out)
        spawnRobots();
    }
}

// Key release callback
void keyboardUp(unsigned char key, int x, int y) {
    activeKeys.erase(key);
}

// Mouse click callback
void mouseClick(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (!isCannonDisabled) {
            fireBullet();
        }
    }
}

// Mouse movement callback with infinite movement support
void mouseMotion(int x, int y) {
    static const int centerX = 400; // Center X position (half of window width)
    static const int centerY = 300; // Center Y position (half of window height)
    static bool justWarped = false;

    // Ignore the mouse event triggered by the warp itself
    if (justWarped) {
        justWarped = false;
        return;
    }

    // Calculate delta movement
    int dx = x - centerX;
    int dy = y - centerY;

    const float verticalLimit = 0.349f; // ~20 degrees in radians

    // Update camera angles based on delta movement
    cameraAngleH += dx * sensitivity;
    cameraAngleV -= dy * sensitivity;

    // Clamp the vertical angle to -Limit to +Limit degrees
    if (cameraAngleV > verticalLimit)
        cameraAngleV = verticalLimit;
    if (cameraAngleV < -verticalLimit)
        cameraAngleV = -verticalLimit;

    // Warp the mouse back to the center of the screen
    glutWarpPointer(centerX, centerY);
    justWarped = true;

    glutPostRedisplay();
}

// Reshape callback
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)w / (double)h, 1.0, planeSize * 3);
}

//// Mesh Importing
// Load mesh from file (within "fps" folder of project)
void loadMesh() {
    const char* folder = "ImportMesh"; // Note: Files and folders NEED to be this name exactly
    const char* fileName = "mesh.obj";

    char filePath[256];

    // Get file path
    snprintf(filePath, sizeof(filePath), "%s/%s", folder, fileName);

    // Open file for reading
    FILE* file;
    if (fopen_s(&file, filePath, "r") != 0 || file == NULL) {
        printf("Could not open file: %s\n", filePath);
        return;
    }

    int vertexCount = 0, quadCount = 0;

    char line[256];

    while (fgets(line, sizeof(line), file)) {
        // Read 1st char of line to determine type (E.g. vertex, normal, face)
        // 
        // Obtain the counts of vertices, quads before allocating arrays for them
        if (strncmp(line, "v ", 2) == 0) {
            vertexCount++;
        }
        else if (strncmp(line, "f ", 2) == 0) {
            quadCount++;
        }
    }

    // Allocate memory for each array of vertices, normals, quads to read to
    Vertex* tempVarray = (Vertex*)malloc(vertexCount * sizeof(Vertex));
    Quad* tempQarray = (Quad*)malloc(quadCount * sizeof(Quad));

    rewind(file); // Set pointer back to beginning

    int vIndex = 0, nIndex = 0, qIndex = 0;

    while (fgets(line, sizeof(line), file)) {
        // Read vertex data (position)
        if (strncmp(line, "v ", 2) == 0) {
            sscanf_s(line, "v %lf %lf %lf",
                &tempVarray[vIndex].x,
                &tempVarray[vIndex].y,
                &tempVarray[vIndex].z);

            vIndex++;
        }
        // Read vertex normal data
        else if (strncmp(line, "vn", 2) == 0) {
            sscanf_s(line, "vn %lf %lf %lf",
                &tempVarray[nIndex].normal.x,
                &tempVarray[nIndex].normal.y,
                &tempVarray[nIndex].normal.z);

            nIndex++;
        }
        // Read quad face data
        // Note: The indices from the file are 1-indexed
        else if (strncmp(line, "f ", 2) == 0) {
            int faceIndices[4];

            // Vertex and Normal index is the same here so we have the same thing twice
            // Just following the .obj file format for faces
                // e.g. "f v/vt/vn"
            sscanf_s(line, "f %d//%d %d//%d %d//%d %d//%d",
                &faceIndices[0], &faceIndices[0],
                &faceIndices[1], &faceIndices[1],
                &faceIndices[2], &faceIndices[2],
                &faceIndices[3], &faceIndices[3]);

            // Obtain each index of the quad
            for (int i = 0; i < 4; i++) {
                tempQarray[qIndex].vertexIndex[i] = faceIndices[i] - 1;
            }

            qIndex++;
        }
    }

    // Update vertex and quad arrays
    // Note: Memory leak happens since we don't free the arrays, but it's only once so it's fine
    varray = tempVarray;
    qarray = tempQarray;

    fclose(file);
    printf("Mesh imported.\n");
}

void drawMeshQuads()
{

    glPushMatrix();
        glTranslatef(0.0f, 5.0f, 0.0f); // Position mesh w/ cannon parts
        //glScalef(1.0f, 1.0f, 1.0f); // Scale mesh if needed
        for (int row = 0; row < 33 - 1; row++)
        {
            for (int col = 0; col < 16; col++)
            {
                glBegin(GL_QUADS);
                for (int i = 0; i < 4; i++)
                {
                    Vertex* vertex = &varray[qarray[row * 16 + col].vertexIndex[i]];

                    // Calculate texture coords
                    float s = (float)col / (16.0f / (float)i);
                    float t = (float)row / (32.0f / (float)i);

                    glTexCoord2f(s, t); // Set texture coords

                    glNormal3f(vertex->normal.x, vertex->normal.y, vertex->normal.z);
                    glVertex3f(vertex->x, vertex->y, vertex->z);
                }
                glEnd();
            }
        }
        glEnd();
    glPopMatrix();
}

// Update in the main function
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1920, 1080);
    glutInitWindowPosition(0, 0);
    glutCreateWindow("A3 - Robot FPS Game");

    glEnable(GL_DEPTH_TEST);

    // Load textures
    planeTexture = loadTexture("land.jpg");
    wallTexture = loadTexture("wall.jpg");
    uiTexture = loadTexture("crosshair.png");
    robotTexture = loadTexture("rough.png");
    gunTexture = loadTexture("gun.jpg");
    cannonTexture = loadTexture("cannon.jpg");
    beltTexture = loadTexture("belt.jpg");

    // Center the cursor at the beginning
    glutWarpPointer(400, 300);
    glutSetCursor(GLUT_CURSOR_NONE); // Hide the cursor for better FPS experience

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp); // Key release callback
    glutMouseFunc(mouseClick); // Mouse click callback
    glutMotionFunc(mouseMotion); // Mouse movement callback with left-click pressed
    glutPassiveMotionFunc(mouseMotion); // Mouse movement callback without button pressed
    glutIdleFunc(handleMovement); // Handle movement continuously

    // Seed random, won't be random otherwise
    srand((unsigned int)time(NULL));

    glutMainLoop();
    return 0;
}

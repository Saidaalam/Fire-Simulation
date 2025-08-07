#include <GL/glut.h>
#include <vector>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <windows.h>
#include <mmsystem.h>
#include <fstream>
#include <direct.h>

// Manual library linking for GCC
#ifdef __GNUC__
#define WINMM_LIB "winmm"
#else
#pragma comment(lib, "winmm.lib")
#endif

// Particle structures
struct FireParticle {
    float x, y;
    float velocity;
    float life;
    float size;
    int windowIndex;
};

struct FireTruck {
    float x;
    bool arrived;
    bool spraying;
    bool leaving;
};

// Simulation states
enum SimState {
    NORMAL,
    FIRE_START,
    ALARM,
    HUMANS_ARRIVE,
    FIREFIGHTERS_ARRIVE,
    EXTINGUISHING,
    ALL_CLEAR,
    TRUCKS_LEAVING
};

// Global variables
SimState currentState = NORMAL;
float simTime = 0.0f;
std::vector<FireParticle> fireParticles;
std::vector<std::string> eventLog;
bool alarmBlinking = false;
float alarmBlinkTimer = 0.0f;
int burningWindow = -1;
FireTruck truck1 = {-100.0f, false, false, false};
FireTruck truck2 = {-150.0f, false, false, false};
float humanPosition = 800.0f;

// Sound flags
bool alarmSoundPlaying = false;
bool truckSoundPlaying = false;
bool waterSoundPlaying = false;

// Colors
GLfloat buildingColors[3][3] = {
    {0.7f, 0.7f, 0.7f},  // Main building
    {0.6f, 0.6f, 0.8f},  // Left building
    {0.8f, 0.6f, 0.6f}   // Right building
};
GLfloat windowColor[3] = {0.8f, 0.9f, 1.0f};
GLfloat fireColors[3][3] = {
    {1.0f, 0.3f, 0.0f},  // Orange
    {1.0f, 0.6f, 0.0f},  // Yellow-orange
    {0.3f, 0.3f, 0.3f}   // Gray (smoke)
};
GLfloat roadColor[3] = {0.2f, 0.2f, 0.2f};
GLfloat treeColors[2][3] = {
    {0.0f, 0.5f, 0.0f},  // Leaves
    {0.4f, 0.2f, 0.0f}   // Trunk
};

// Sound helper functions
bool checkSoundFile(const char* filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.good()) {
        printf("SOUND ERROR: File '%s' not found!\n", filename);
        return false;
    }
    file.close();
    return true;
}

void playAlarmSound() {
    if (!alarmSoundPlaying) {
        if (checkSoundFile("fire.wav")) {
            if (!PlaySoundA("fire.wav", NULL, SND_ASYNC | SND_LOOP | SND_FILENAME)) {
                printf("Failed to play alarm! Error: %lu\n", (unsigned long)GetLastError());
            } else {
                alarmSoundPlaying = true;
            }
        }
    }
}

void stopAlarmSound() {
    PlaySound(NULL, NULL, 0);
    alarmSoundPlaying = false;
}

void playTruckSound() {
    if (!truckSoundPlaying) {
        if (checkSoundFile("TruckArrive.wav")) {
            if (!PlaySoundA("TruckArrive.wav", NULL, SND_ASYNC | SND_LOOP | SND_FILENAME)) {
                printf("Failed to play truck! Error: %lu\n", (unsigned long)GetLastError());
            } else {
                truckSoundPlaying = true;
            }
        }
    }
}

void stopTruckSound() {
    PlaySound(NULL, NULL, 0);
    truckSoundPlaying = false;
}

void playWaterSound() {
    if (!waterSoundPlaying) {
        if (checkSoundFile("WaterSpray.wav")) {
            if (!PlaySoundA("WaterSpray.wav", NULL, SND_ASYNC | SND_LOOP | SND_FILENAME)) {
                printf("Failed to play water! Error: %lu\n", (unsigned long)GetLastError());
            } else {
                waterSoundPlaying = true;
            }
        }
    }
}

void stopWaterSound() {
    PlaySound(NULL, NULL, 0);
    waterSoundPlaying = false;
}

void init() {
    glClearColor(0.53f, 0.81f, 0.98f, 1.0f); // Sky blue

    // Initial event log
    eventLog.push_back("System: Simulation started");
    eventLog.push_back("System: Normal operation");

    srand(static_cast<unsigned int>(time(NULL)));
}

void drawSky() {
    // Gradient sky
    glBegin(GL_QUADS);
    glColor3f(0.53f, 0.81f, 0.98f); // Top color
    glVertex2f(0, 0);
    glVertex2f(800, 0);
    glColor3f(0.7f, 0.9f, 1.0f);    // Bottom color
    glVertex2f(800, 500);
    glVertex2f(0, 500);
    glEnd();
}

void drawCloud(float x, float y, float size) {
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_POLYGON);
    for (int i = 0; i < 20; i++) {
        float angle = 2.0f * 3.14159f * i / 20;
        glVertex2f(x + size * cos(angle), y + size * sin(angle));
    }
    glEnd();
}

void drawRoad() {
    // Main road
    glColor3fv(roadColor);
    glBegin(GL_QUADS);
    glVertex2f(0, 400);
    glVertex2f(800, 400);
    glVertex2f(800, 450);
    glVertex2f(0, 450);
    glEnd();

    // Road markings
    glColor3f(1.0f, 1.0f, 1.0f);
    for (int i = 0; i < 8; i++) {
        glBegin(GL_QUADS);
        glVertex2f(50 + i * 100, 425);
        glVertex2f(80 + i * 100, 425);
        glVertex2f(80 + i * 100, 430);
        glVertex2f(50 + i * 100, 430);
        glEnd();
    }
}

void drawBuilding(int index, float x, float width, float height, int floors, int windowsPerFloor) {
    // Building structure
    glColor3fv(buildingColors[index]);
    glBegin(GL_QUADS);
    glVertex2f(x, 400);
    glVertex2f(x + width, 400);
    glVertex2f(x + width, 400 - height);
    glVertex2f(x, 400 - height);
    glEnd();

    // Windows
    float windowWidth = width / (windowsPerFloor + 1);
    float windowHeight = height / (floors + 1) * 0.6f;
    float floorHeight = height / floors;

    for (int floor = 0; floor < floors; floor++) {
        for (int col = 0; col < windowsPerFloor; col++) {
            // Determine if this window is on fire
            bool isBurning = (index == 0 && currentState >= FIRE_START && currentState < ALL_CLEAR &&
                             burningWindow == floor * windowsPerFloor + col);

            if (isBurning) {
                glColor3f(1.0f, 0.5f, 0.0f); // Fire glow
            } else {
                glColor3fv(windowColor);
            }

            glBegin(GL_QUADS);
            float winX = x + (col + 0.5f) * windowWidth;
            float winY = 400 - (floor + 0.5f) * floorHeight;
            glVertex2f(winX - windowWidth*0.4f, winY - windowHeight*0.5f);
            glVertex2f(winX + windowWidth*0.4f, winY - windowHeight*0.5f);
            glVertex2f(winX + windowWidth*0.4f, winY + windowHeight*0.5f);
            glVertex2f(winX - windowWidth*0.4f, winY + windowHeight*0.5f);
            glEnd();

            // Window frame
            glColor3f(0.3f, 0.3f, 0.3f);
            glLineWidth(1.0f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(winX - windowWidth*0.4f, winY - windowHeight*0.5f);
            glVertex2f(winX + windowWidth*0.4f, winY - windowHeight*0.5f);
            glVertex2f(winX + windowWidth*0.4f, winY + windowHeight*0.5f);
            glVertex2f(winX - windowWidth*0.4f, winY + windowHeight*0.5f);
            glEnd();
        }
    }

    // Roof
    if (index == 0) { // Only main building has a special roof
        glColor3f(0.4f, 0.4f, 0.4f);
        glBegin(GL_QUADS);
        glVertex2f(x - 10, 400 - height);
        glVertex2f(x + width + 10, 400 - height);
        glVertex2f(x + width, 400 - height - 20);
        glVertex2f(x, 400 - height - 20);
        glEnd();
    }
}

void drawTrees() {
    // Draw trees along the road
    for (int i = 0; i < 15; i++) {
        float x = 50 + i * 50;
        if (x > 250 && x < 550) continue; // Don't draw trees behind main building

        // Trunk
        glColor3fv(treeColors[1]);
        glBegin(GL_QUADS);
        glVertex2f(x - 5, 400);
        glVertex2f(x + 5, 400);
        glVertex2f(x + 5, 370);
        glVertex2f(x - 5, 370);
        glEnd();

        // Leaves
        glColor3fv(treeColors[0]);
        glBegin(GL_POLYGON);
        for (int j = 0; j < 20; j++) {
            float angle = 2.0f * 3.14159f * j / 20;
            glVertex2f(x + 15 * cos(angle), 370 + 15 * sin(angle));
        }
        glEnd();
    }
}

void updateFireParticles(float deltaTime) {
    // Remove dead particles
    fireParticles.erase(
        std::remove_if(fireParticles.begin(), fireParticles.end(),
            [](const FireParticle& p) { return p.life <= 0.0f; }),
        fireParticles.end());

    // Update existing particles
    for (auto& p : fireParticles) {
        p.y += p.velocity * deltaTime * 50.0f;
        p.x += (sin(simTime * 2.0f + p.x) * 0.5f * deltaTime * 50.0f);
        p.life -= 0.5f * deltaTime;
    }

    // Add new particles if fire is active
    if (currentState >= FIRE_START && currentState < ALL_CLEAR && burningWindow != -1) {
        int windowsPerFloor = 5;
        int floor = burningWindow / windowsPerFloor;
        int col = burningWindow % windowsPerFloor;

        float windowWidth = 100.0f / (windowsPerFloor + 1);
        float floorHeight = 150.0f / 5;
        float winX = 300 + (col + 0.5f) * windowWidth;
        float winY = 400 - (floor + 0.5f) * floorHeight;

        for (int i = 0; i < 5; i++) {
            FireParticle p;
            p.x = winX + (rand() % 100 - 50) / 20.0f;
            p.y = winY + (rand() % 100) / 20.0f;
            p.velocity = 0.5f + (rand() % 100) / 100.0f;
            p.life = 0.5f + (rand() % 100) / 200.0f;
            p.size = 2.0f + (rand() % 10) / 5.0f;
            p.windowIndex = burningWindow;
            fireParticles.push_back(p);
        }
    }
}

void drawFire() {
    if (currentState < FIRE_START || currentState >= ALL_CLEAR) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (const auto& p : fireParticles) {
        // Determine color based on particle life
        if (p.life > 0.7f) {
            glColor4f(fireColors[0][0], fireColors[0][1], fireColors[0][2], 0.8f);
        } else if (p.life > 0.3f) {
            glColor4f(fireColors[1][0], fireColors[1][1], fireColors[1][2], p.life);
        } else {
            glColor4f(fireColors[2][0], fireColors[2][1], fireColors[2][2], p.life * 0.5f);
        }

        // Draw particle
        glPointSize(p.size);
        glBegin(GL_POINTS);
        glVertex2f(p.x, p.y);
        glEnd();
    }

    glDisable(GL_BLEND);
}

void drawHumans() {
    if (currentState < HUMANS_ARRIVE) return;

    glColor3f(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 3; i++) {
        float x = humanPosition + i * 30;
        float y = 380 + sin(simTime * 2.0f + i) * 5.0f;

        // Head
        glPointSize(6.0f);
        glBegin(GL_POINTS);
        glVertex2f(x, y);
        glEnd();

        // Body
        glBegin(GL_LINES);
        glVertex2f(x, y);
        glVertex2f(x, y + 15);
        glEnd();

        // Legs
        glBegin(GL_LINES);
        glVertex2f(x, y + 15);
        glVertex2f(x - 5, y + 25);
        glVertex2f(x, y + 15);
        glVertex2f(x + 5, y + 25);
        glEnd();

        // Arms
        float armAngle = sin(simTime * 5.0f + i) * 30.0f;
        glPushMatrix();
        glTranslatef(x, y + 10, 0.0f);
        glRotatef(armAngle, 0.0f, 0.0f, 1.0f);
        glBegin(GL_LINES);
        glVertex2f(0, 0);
        glVertex2f(-10, 0);
        glVertex2f(0, 0);
        glVertex2f(10, 0);
        glEnd();
        glPopMatrix();
    }
}

void drawFireTruck(const FireTruck& truck) {
    // Truck body
    glColor3f(1.0f, 0.5f, 0.f);
    glBegin(GL_QUADS);
    glVertex2f(truck.x, 370);
    glVertex2f(truck.x + 60, 370);
    glVertex2f(truck.x + 60, 400);
    glVertex2f(truck.x, 400);
    glEnd();

    // Cabin
    glColor3f(0.9f, 0.9f, 0.9f);
    glBegin(GL_QUADS);
    glVertex2f(truck.x + 40, 370);
    glVertex2f(truck.x + 60, 370);
    glVertex2f(truck.x + 60, 390);
    glVertex2f(truck.x + 40, 390);
    glEnd();

    // Wheels
    glColor3f(0.1f, 0.1f, 0.1f);
    for (int i = 0; i < 2; i++) {
        glBegin(GL_POLYGON);
        for (int j = 0; j < 20; j++) {
            float angle = 2.0f * 3.14159f * j / 20;
            glVertex2f(truck.x + 15 + i * 30 + 10 * cos(angle), 400 + 10 * sin(angle));
        }
        glEnd();
    }

    // Water hose when spraying
    if (truck.spraying) {
        int windowsPerFloor = 5;
        int floor = burningWindow / windowsPerFloor;
        int col = burningWindow % windowsPerFloor;
        float windowWidth = 100.0f / (windowsPerFloor + 1);
        float floorHeight = 150.0f / 5;
        float targetX = 300 + (col + 0.5f) * windowWidth;
        float targetY = 400 - (floor + 0.5f) * floorHeight;

        glColor4f(0.2f, 0.5f, 1.0f, 0.6f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glVertex2f(truck.x + 30, 385);
        glVertex2f(targetX, targetY);
        glEnd();
        glLineWidth(1.0f);
    }
}

void updateFireTrucks(float deltaTime) {
    // Truck 1 logic - slower movement (0.8 units/frame)
    if (currentState == FIREFIGHTERS_ARRIVE && !truck1.arrived) {
        truck1.x = std::min(200.0f, truck1.x + 0.8f);
        if (truck1.x >= 200.0f) {
            truck1.arrived = true;
        }
    } else if (currentState == EXTINGUISHING && truck1.arrived) {
        truck1.spraying = true;
    } else if (currentState == TRUCKS_LEAVING && !truck1.leaving) {
        truck1.spraying = false;
        truck1.leaving = true;
    }

    if (truck1.leaving) {
        truck1.x += 1.5f; // Slower leaving speed
    }

    // Truck 2 logic - slower movement (0.8 units/frame)
    if (currentState == FIREFIGHTERS_ARRIVE && truck1.x > 150.0f && !truck2.arrived) {
        truck2.x = std::min(250.0f, truck2.x + 0.8f);
        if (truck2.x >= 250.0f) {
            truck2.arrived = true;
        }
    } else if (currentState == EXTINGUISHING && truck2.arrived) {
        truck2.spraying = true;
    } else if (currentState == TRUCKS_LEAVING && !truck2.leaving) {
        truck2.spraying = false;
        truck2.leaving = true;
    }

    if (truck2.leaving) {
        truck2.x += 1.5f; // Slower leaving speed
    }
}

void updateHumans(float deltaTime) {
    if (currentState >= HUMANS_ARRIVE && currentState < FIREFIGHTERS_ARRIVE) {
        humanPosition = std::max(350.0f, humanPosition - 0.5f); // Humans move in from right
    }
}

void drawAlarm() {
    if (currentState < ALARM || currentState >= ALL_CLEAR) return;

    alarmBlinkTimer += 0.1f;
    if (alarmBlinkTimer > 0.5f) {
        alarmBlinking = !alarmBlinking;
        alarmBlinkTimer = 0.0f;
    }

    if (alarmBlinking) {
        glColor3f(1.0f, 0.0f, 0.0f);
        glBegin(GL_QUADS);
        glVertex2f(280, 250);
        glVertex2f(290, 250);
        glVertex2f(290, 260);
        glVertex2f(280, 260);
        glEnd();
    }
}

void drawInterface() {
    // Status panel background
    glColor4f(0.0f, 0.0f, 0.0f, 0.7f);
    glBegin(GL_QUADS);
    glVertex2f(10, 10);
    glVertex2f(300, 10);
    glVertex2f(300, 120);
    glVertex2f(10, 120);
    glEnd();

    // Status text
    glColor3f(1.0f, 1.0f, 1.0f);
    std::string statusText;
    switch(currentState) {
        case NORMAL: statusText = "Status: Normal"; break;
        case FIRE_START: statusText = "ALERT: Fire detected!"; glColor3f(1.0f, 0.0f, 0.0f); break;
        case ALARM: statusText = "ALERT: Alarm activated!"; glColor3f(1.0f, 0.0f, 0.0f); break;
        case HUMANS_ARRIVE: statusText = "Emergency crew arriving"; glColor3f(0.0f, 1.0f, 0.0f); break;
        case FIREFIGHTERS_ARRIVE: statusText = "Firefighters arriving"; glColor3f(0.0f, 1.0f, 0.0f); break;
        case EXTINGUISHING: statusText = "Extinguishing fire"; glColor3f(0.0f, 0.5f, 1.0f); break;
        case ALL_CLEAR: statusText = "ALL CLEAR - Fire out"; glColor3f(0.0f, 1.0f, 0.0f); break;
        case TRUCKS_LEAVING: statusText = "Firefighters leaving"; glColor3f(0.0f, 1.0f, 0.0f); break;
    }

    glRasterPos2f(20, 30);
    for (char c : statusText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    // Timer
    glColor3f(1.0f, 1.0f, 1.0f);
    char timeStr[20];
    snprintf(timeStr, sizeof(timeStr), "Time: %.1fs", simTime);
    glRasterPos2f(20, 60);
    for (char c : std::string(timeStr)) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    // Latest events
    int startIdx = eventLog.size() > 3 ? eventLog.size() - 3 : 0;
    for (int i = 0; i < 3 && startIdx + i < eventLog.size(); i++) {
        glRasterPos2f(20, 90 + i * 20);
        for (char c : eventLog[startIdx + i]) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
        }
    }
}

void updateSimulation(float deltaTime) {
    simTime += deltaTime;

    // State transitions
    if (currentState == NORMAL && simTime > 3.0f) {
        currentState = FIRE_START;
        burningWindow = rand() % 15; // 3 floors * 5 windows
        eventLog.push_back("ALERT: Fire detected in building!");
    }
    else if (currentState == FIRE_START && simTime > 6.0f) {
        currentState = ALARM;
        eventLog.push_back("ALERT: Alarm activated!");
        playAlarmSound();
    }
    else if (currentState == ALARM && simTime > 9.0f) {
        currentState = HUMANS_ARRIVE;
        eventLog.push_back("UPDATE: Emergency crew arriving");
    }
    else if (currentState == HUMANS_ARRIVE && humanPosition <= 350.0f && simTime > 12.0f) {
        currentState = FIREFIGHTERS_ARRIVE;
        eventLog.push_back("UPDATE: Firefighters dispatched");
        playTruckSound();
    }
    else if (currentState == FIREFIGHTERS_ARRIVE && truck1.arrived && truck2.arrived && simTime > 15.0f) {
        currentState = EXTINGUISHING;
        eventLog.push_back("UPDATE: Firefighters extinguishing fire");
        playWaterSound();
    }
    else if (currentState == EXTINGUISHING && simTime > 25.0f) {
        currentState = ALL_CLEAR;
        eventLog.push_back("UPDATE: Fire extinguished!");
        stopAlarmSound();
        stopWaterSound();
    }
    else if (currentState == ALL_CLEAR && simTime > 28.0f) {
        currentState = TRUCKS_LEAVING;
        eventLog.push_back("UPDATE: Firefighters leaving scene");
        stopTruckSound();
    }

    // Update all elements
    updateHumans(deltaTime);
    updateFireTrucks(deltaTime);
    if (currentState >= FIRE_START && currentState < ALL_CLEAR) {
        updateFireParticles(deltaTime);
    }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    // Set up 3D orthographic projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, 800, 500, 0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Draw scene
    drawSky();
    drawCloud(100, 80, 30);
    drawCloud(500, 120, 40);
    drawCloud(700, 60, 25);
    drawRoad();
    drawTrees();

    // Buildings (left, main, right)
    drawBuilding(1, 100, 80, 120, 4, 3);  // Left building
    drawBuilding(0, 300, 100, 150, 5, 5); // Main building
    drawBuilding(2, 500, 90, 130, 4, 4);  // Right building

    // Draw all elements in proper order
    drawFire();
    drawHumans(); // Humans appear before trucks
    drawFireTruck(truck1);
    drawFireTruck(truck2);
    drawAlarm();
    drawInterface();

    glutSwapBuffers();
}

void idle() {
    static int lastTime = 0;
    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    float deltaTime = (currentTime - lastTime) / 1000.0f;
    lastTime = currentTime;

    updateSimulation(deltaTime);
    glutPostRedisplay();
}

void cleanup() {
    stopAlarmSound();
    stopTruckSound();
    stopWaterSound();
}

int main(int argc, char** argv) {
    // Debug: Print working directory
    char cwd[1024];
    if (_getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current directory: %s\n", cwd);
    }

    // Verify sound files
    checkSoundFile("FireAlarm.wav");
    checkSoundFile("TruckArrive.wav");
    checkSoundFile("WaterSpray.wav");

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 500);
    glutCreateWindow("3D Fire Emergency Simulation");

    init();

    glutDisplayFunc(display);
    glutIdleFunc(idle);
    atexit([]() {
        stopAlarmSound();
        stopTruckSound();
        stopWaterSound();
    });

    glutMainLoop();
    return 0;
}

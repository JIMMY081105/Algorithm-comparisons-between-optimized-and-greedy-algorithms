#include "core/Truck.h"

Truck::Truck()
    : posX(0.0f), posY(0.0f), speed(1.0f),
      currentSegment(0), segmentProgress(0.0f), moving(false) {}

float Truck::getPosX() const { return posX; }
float Truck::getPosY() const { return posY; }
void Truck::setPosition(float x, float y) { posX = x; posY = y; }

float Truck::getSpeed() const { return speed; }
void Truck::setSpeed(float s) {
    // Keep speed within a reasonable range for the animation
    if (s < 0.1f) s = 0.1f;
    if (s > 5.0f) s = 5.0f;
    speed = s;
}

int Truck::getCurrentSegment() const { return currentSegment; }
void Truck::setCurrentSegment(int seg) { currentSegment = seg; }
float Truck::getSegmentProgress() const { return segmentProgress; }
void Truck::setSegmentProgress(float p) { segmentProgress = p; }

bool Truck::isMoving() const { return moving; }
void Truck::setMoving(bool m) { moving = m; }

void Truck::resetToStart(float hqX, float hqY) {
    posX = hqX;
    posY = hqY;
    currentSegment = 0;
    segmentProgress = 0.0f;
    moving = false;
}

bool Truck::advanceProgress(float deltaTime) {
    if (!moving) return false;

    // The rate at which we traverse each segment depends on speed.
    // A base rate of 0.8 per second at speed=1 feels visually smooth.
    float advanceRate = 0.8f * speed;
    segmentProgress += advanceRate * deltaTime;

    if (segmentProgress >= 1.0f) {
        segmentProgress = 1.0f;
        return true;  // signal that this segment is complete
    }
    return false;
}

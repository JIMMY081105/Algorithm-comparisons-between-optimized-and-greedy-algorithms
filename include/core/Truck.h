#ifndef TRUCK_H
#define TRUCK_H

// Manages the visual state of the collection truck during route animation.
// The truck moves along a sequence of waypoints using linear interpolation,
// and the AnimationController drives its position updates each frame.
class Truck {
private:
    float posX;             // current truck world-space x position
    float posY;             // current truck world-space y position
    float speed;            // movement speed multiplier
    int currentSegment;     // which segment of the route we're on
    float segmentProgress;  // 0.0 to 1.0 — how far along current segment
    bool moving;            // whether the truck is actively animating

public:
    Truck();

    // Position
    float getPosX() const;
    float getPosY() const;
    void setPosition(float x, float y);

    // Speed control
    float getSpeed() const;
    void setSpeed(float s);

    // Segment tracking for route animation
    int getCurrentSegment() const;
    void setCurrentSegment(int seg);
    float getSegmentProgress() const;
    void setSegmentProgress(float p);

    // Movement state
    bool isMoving() const;
    void setMoving(bool m);

    // Reset truck to starting position at HQ
    void resetToStart(float hqX, float hqY);

    // Advance the truck's progress along the current segment.
    // Returns true if the segment is complete and we should move to the next.
    bool advanceProgress(float deltaTime);
};

#endif // TRUCK_H

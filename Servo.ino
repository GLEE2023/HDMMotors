#include <Servo.h>

Servo leftServo;
Servo rightServo;

const int servoLeftPin = 4;
const int servoRightPin = 3;

// Rest positions
const int LEFT_REST  = 180;
const int RIGHT_REST = 0;

// Fire positions
const int LEFT_FIRE  = 60;     // CCW
const int RIGHT_FIRE = 120;   // CW

int leftAngle = LEFT_REST;
int rightAngle = RIGHT_REST;

void writeServos(int left, int right)
{
    leftServo.write(constrain(left, 0, 180));
    rightServo.write(constrain(right, 0, 180));
}


void moveServosSmooth(int targetLeft, int targetRight)
{
    const int step = 2;
    const int wait = 10;

    while (leftAngle != targetLeft || rightAngle != targetRight)
    {
        if (leftAngle < targetLeft)
            leftAngle += step;
        else if (leftAngle > targetLeft)
            leftAngle -= step;

        if (rightAngle < targetRight)
            rightAngle += step;
        else if (rightAngle > targetRight)
            rightAngle -= step;

        if (abs(leftAngle - targetLeft) < step)
            leftAngle = targetLeft;

        if (abs(rightAngle - targetRight) < step)
            rightAngle = targetRight;

        writeServos(leftAngle, rightAngle);
        delay(wait);
    }
}

void printControls()
{
    Serial.println();
    Serial.println("===== Pinball Servo Controller =====");
    Serial.println("w = reset");
    Serial.println("e = arm");
    Serial.println("p = fire");
    Serial.println("====================================");
}

void setup()
{
    Serial.begin(115200);

    leftServo.attach(servoLeftPin);
    rightServo.attach(servoRightPin);

    writeServos(LEFT_REST, RIGHT_REST);

    delay(500);
    printControls();
}

void loop()
{
    if (!Serial.available())
        return;

    char command = Serial.read();

    switch (command)
    {
        case 'w':
            moveServosSmooth(LEFT_REST, RIGHT_REST);
            break;

        case 'e':
            moveServosSmooth(135, 45);
            break;

        case 'p':
            moveServosSmooth(LEFT_FIRE, RIGHT_FIRE);
            delay(250);
            moveServosSmooth(LEFT_REST, RIGHT_REST);
            break;

        case '?':
        case 'h':
            printControls();
            break;

        case '\n':
        case '\r':
            return;
    }

    Serial.print("Left: ");
    Serial.print(leftAngle);
    Serial.print("  Right: ");
    Serial.println(rightAngle);
}

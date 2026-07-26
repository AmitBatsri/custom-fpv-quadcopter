float normalize(int raw, int center) {
    float result;

    if (raw >= center) {
        int span = MAX_VAL - center;
        if (span <= 0) return 0.0;              
        result = (float)(raw - center) / span;
    } else {
        int span = center - MIN_VAL;
        if (span <= 0) return 0.0;              
        result = (float)(raw - center) / span;
    }
    if (result >  1.0) result =  1.0;
    if (result < -1.0) result = -1.0;
    return result;
}

LeftJoystickOutput JoystickOut(int raw_x, int raw_y) {
    float nx = normalize(raw_x, LEFT_CENTER_X);
    float ny = normalize(raw_y, LEFT_CENTER_Y);

    if (abs(nx) < DEADBAND) nx = 0;
    if (abs(ny) < DEADBAND) ny = 0;

    LeftJoystickOutput result;
    result.nx = nx;
    result.ny = ny;
    return result;
}

RightJoystickOutput RightJoystickOut(int raw_x, int raw_y) {
    float nx = normalize(raw_x, RIGHT_CENTER_X);
    float ny = normalize(raw_y, RIGHT_CENTER_Y);

    if (abs(nx) < DEADBAND) nx = 0;
    if (abs(ny) < DEADBAND) ny = 0;

    RightJoystickOutput result;
    result.roll = roll_set(nx);
    result.pitch = pitch_set(ny);
    return result;
}

int throttle_set(float y, int current_throttle) {
    int new_throttle = current_throttle + (int)(y * MAX_THROTTLE_INCREMENT);
    return constrain(new_throttle, 0, 100);
}

int yaw_set(float x) {
    if (x == 0) return 0;
    return (int)(x * (-45));
}

int roll_set(float x) {
    if (x == 0) return 0;
    return (int)(x * (-45));
}

int pitch_set(float y) {
    if (y == 0) return 0;
    return (int)(y * 45);
}
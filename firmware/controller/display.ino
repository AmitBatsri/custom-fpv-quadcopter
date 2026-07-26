bool display_init() {
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("Display not found (SSD1306 @ 0x3C)");
        return false;               
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.display();
    return true;
}

void display_test_mode() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);

    display.setCursor(0, 0);
    display.print("THR: ");
    display.print(throttle);

    display.setCursor(70, 0);
    display.print("YAW: ");
    display.print(yaw);

    display.setCursor(0, 16);
    display.print("ROL: ");
    display.print(roll);

    display.setCursor(70, 16);
    display.print("PIT: ");
    display.print(pitch);

    display.setCursor(0, 32);
    display.print("MODE: ");
    display.print(stabilized ? "STAB" : "ACRO");

    display.setCursor(0, 48);
    display.print("DISARMED (test)");

    display.display();
}

void display_flight_mode() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);

    display.setCursor(0, 0);
    display.print("MODE: ");
    display.print(stabilized ? "STABILIZED" : "ACRO");

    display.setCursor(0, 16);
    display.print("ARM:  ");
    display.print(armed ? "ARMED" : "DISARMED");

    display.setCursor(0, 32);
    display.print("ALT:  ");
    display.print(altitude, 1);       
    display.print("m");

    display.setCursor(0, 48);
    if (link_lost) {
        display.print("LINK LOST");
    } else {
        display.print("BAT:");
        display.print(battery, 1);
        display.print("V LNK:");
        display.print(link_quality);
        display.print("%");
    }

    display.display();
}
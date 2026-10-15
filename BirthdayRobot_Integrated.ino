#include <Wire.h>

#include <Adafruit_GFX.h>

#include <Adafruit_SSD1306.h>

// =====================================================

// HARDWARE

// Arduino Nano: OLED SDA=A4, SCL=A5; button D3 -> GND

// =====================================================

#define SCREEN_WIDTH 128

#define SCREEN_HEIGHT 64

#define OLED_RESET -1

#define SCREEN_ADDRESS 0x3C

#define BUTTON_PIN 3

#define BUZZER_PIN 9

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool oledOK = false;

bool birthdayMode = false;

// Birthday sequence state. Existing eye-animation state remains unchanged.
enum BirthdayStage { BD_IDLE, BD_SURPRISE, BD_HAPPY, BD_HEART, BD_CAKE, BD_MESSAGES };
BirthdayStage birthdayStage = BD_IDLE;
unsigned long birthdayStageStarted = 0;
unsigned long birthdayFrameAt = 0;
int birthdayFrame = 0;
int messageScreen = 0;

const unsigned long debounceDelay = 35;

int rawButtonState = HIGH;

int stableButtonState = HIGH;

unsigned long lastButtonChange = 0;

unsigned long pressCount = 0;

// Happy Birthday melody for a PASSIVE piezo buzzer.

// Notes/durations are advanced with millis(), so button checks stay responsive.

const int birthdayNotes[] = {

  392, 392, 440, 392, 523, 494,

  392, 392, 440, 392, 587, 523,

  392, 392, 784, 659, 523, 494, 440,

  698, 698, 659, 523, 587, 523

};

const unsigned int birthdayDurations[] = {

  350, 350, 700, 700, 700, 1200,

  350, 350, 700, 700, 700, 1200,

  350, 350, 700, 700, 700, 700, 1200,

  350, 350, 700, 700, 700, 1400

};

const int birthdayNoteCount = sizeof(birthdayNotes) / sizeof(birthdayNotes[0]);

int birthdayNoteIndex = 0;

bool buzzerNoteOn = false;

unsigned long buzzerNextAt = 0;

// Non-blocking startup sequence: splash (2 s), sleeping eyes (2 s), demo.

int startupStage = 1;

unsigned long startupStageStarted = 0;

// Original animation settings

int demo_mode = 1;

static const int max_animation_index = 8;

int current_animation_index = 0;

// Original eye geometry

int ref_eye_height = 40;

int ref_eye_width = 40;

int ref_space_between_eye = 10;

int ref_corner_radius = 10;

int left_eye_height = ref_eye_height;

int left_eye_width = ref_eye_width;

int left_eye_x = 32;

int left_eye_y = 32;

int right_eye_x = 32 + ref_eye_width + ref_space_between_eye;

int right_eye_y = 32;

int right_eye_height = ref_eye_height;

int right_eye_width = ref_eye_width;

// =====================================================

// NON-BLOCKING ANIMATION STATE

// =====================================================

enum AnimKind {

  ANIM_NONE,

  ANIM_WAKEUP,

  ANIM_CENTER,

  ANIM_MOVE_RIGHT,

  ANIM_MOVE_LEFT,

  ANIM_BLINK,

  ANIM_HAPPY,

  ANIM_SLEEP,

  ANIM_SACCADES

};

enum AnimPhase {

  PHASE_NONE,

  PHASE_WAKEUP_OPEN,

  PHASE_BLINK_CLOSE,

  PHASE_BLINK_OPEN,

  PHASE_MOVE_OUT_CLOSE,

  PHASE_MOVE_OUT_OPEN,

  PHASE_MOVE_HOLD,

  PHASE_MOVE_BACK_CLOSE,

  PHASE_MOVE_BACK_OPEN,

  PHASE_HAPPY_DRAW,

  PHASE_HAPPY_HOLD,

  PHASE_SACCADE_OUT,

  PHASE_SACCADE_BACK

};

AnimKind animKind = ANIM_NONE;

AnimPhase animPhase = PHASE_NONE;

bool animationRunning = false;

bool manualAnimation = false;

unsigned long lastFrameAt = 0;

unsigned long phaseStartedAt = 0;

unsigned long frameInterval = 1;

int phaseStep = 0;

int blinkSpeed = 12;

int saccadesDone = 0;

int saccadeTargetX = 0;

int saccadeTargetY = 0;

int saccadeOriginXLeft = 0;

int saccadeOriginXRight = 0;

int saccadeOriginY = 0;

// Keep happy-eye's original triangle offset animation.

int happyOffset = 0;

// =====================================================

// OLED DRAWING (same geometry as original)

// =====================================================

void draw_eyes(bool update = true) {

  if (!oledOK) return;

  display.clearDisplay();

  int x = int(left_eye_x - left_eye_width / 2);

  int y = int(left_eye_y - left_eye_height / 2);

  display.fillRoundRect(

    x, y, left_eye_width, left_eye_height,

    ref_corner_radius, SSD1306_WHITE

  );

  x = int(right_eye_x - right_eye_width / 2);

  y = int(right_eye_y - right_eye_height / 2);

  display.fillRoundRect(

    x, y, right_eye_width, right_eye_height,

    ref_corner_radius, SSD1306_WHITE

  );

  if (update) display.display();

}

void center_eyes(bool update = true) {

  left_eye_height = ref_eye_height;

  left_eye_width = ref_eye_width;

  right_eye_height = ref_eye_height;

  right_eye_width = ref_eye_width;

  left_eye_x = SCREEN_WIDTH / 2 - ref_eye_width / 2 - ref_space_between_eye / 2;

  left_eye_y = SCREEN_HEIGHT / 2;

  right_eye_x = SCREEN_WIDTH / 2 + ref_eye_width / 2 + ref_space_between_eye / 2;

  right_eye_y = SCREEN_HEIGHT / 2;

  draw_eyes(update);

}

// Birthday greeting renderer is defined with the birthday-mode screens below.

// =====================================================

// NON-BLOCKING BUTTON DEBOUNCE

// =====================================================

bool buttonPressed() {

  int reading = digitalRead(BUTTON_PIN);

  if (reading != rawButtonState) {

    rawButtonState = reading;

    lastButtonChange = millis();

  }

  if (millis() - lastButtonChange >= debounceDelay) {

    if (stableButtonState != rawButtonState) {

      stableButtonState = rawButtonState;

      if (stableButtonState == LOW) return true;

    }

  }

  return false;

}

// =====================================================

// NON-BLOCKING HAPPY BIRTHDAY MELODY

// =====================================================

void startBirthdayMelody() {

  birthdayNoteIndex = 0;

  buzzerNoteOn = false;

  buzzerNextAt = millis();

  noTone(BUZZER_PIN);

}

void stopBirthdayMelody() {

  noTone(BUZZER_PIN);

  buzzerNoteOn = false;

  birthdayNoteIndex = 0;

}

void updateBirthdayMelody() {

  if (!birthdayMode) return;

  unsigned long now = millis();

  if (now < buzzerNextAt) return;

  if (buzzerNoteOn) {

    noTone(BUZZER_PIN);

    buzzerNoteOn = false;

    buzzerNextAt = now + 45;  // gap between notes

    birthdayNoteIndex++;

    if (birthdayNoteIndex >= birthdayNoteCount) {

      birthdayNoteIndex = birthdayNoteCount; // play once

    }

    return;

  }

  if (birthdayNoteIndex >= birthdayNoteCount) return;

  tone(BUZZER_PIN, birthdayNotes[birthdayNoteIndex]);

  buzzerNoteOn = true;

  buzzerNextAt = now + birthdayDurations[birthdayNoteIndex];

}

// =====================================================

// OLED DIAGNOSTICS / RECOVERY

// =====================================================

void checkI2C() {

  Wire.beginTransmission(SCREEN_ADDRESS);

  byte error = Wire.endTransmission();

  Serial.print(F("OLED 0x3C: "));

  Serial.println(error == 0 ? F("DEVICE FOUND") : F("NO RESPONSE"));

}

void restartOLED() {

  Serial.println(F("Reinitializing OLED..."));

  oledOK = display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);

  Wire.setClock(100000);

  if (!oledOK) {

    Serial.println(F("OLED initialization FAILED"));

    return;

  }

  Serial.println(F("OLED initialized"));

  if (birthdayMode) {
    if (birthdayStage == BD_MESSAGES) showBirthdayMessage();
    else birthdayFrameAt = 0;
  } else draw_eyes(true);

}

// =====================================================

// ANIMATION START / FINISH

// =====================================================

void finishAnimation() {

  animationRunning = false;

  animKind = ANIM_NONE;

  animPhase = PHASE_NONE;

  if (!manualAnimation && !birthdayMode && demo_mode == 1) {

    current_animation_index++;

    if (current_animation_index > max_animation_index) {

      current_animation_index = 0;

    }

  }

}

void startAnimation(int index, bool manual = false) {

  if (index < 0 || index > max_animation_index) index = 8;

  if (manual) demo_mode = 0;

  manualAnimation = manual;

  animationRunning = true;

  phaseStep = 0;

  saccadesDone = 0;

  lastFrameAt = millis();

  phaseStartedAt = millis();

  switch (index) {

    case 0: // Original wakeup(): sleep(), then height grows 0..40 by 2.

      animKind = ANIM_WAKEUP;

      animPhase = PHASE_WAKEUP_OPEN;

      left_eye_height = 0;

      right_eye_height = 0;

      frameInterval = 1;

      draw_eyes(true);

      break;

    case 1: // Original center_eyes(true)

      center_eyes(true);

      finishAnimation();

      break;

    case 2: // Original move_right_big_eye()

      center_eyes(false);

      animKind = ANIM_MOVE_RIGHT;

      animPhase = PHASE_MOVE_OUT_CLOSE;

      frameInterval = 1;

      break;

    case 3: // Original move_left_big_eye()

      center_eyes(false);

      animKind = ANIM_MOVE_LEFT;

      animPhase = PHASE_MOVE_OUT_CLOSE;

      frameInterval = 1;

      break;

    case 4: // Original blink(10)

      animKind = ANIM_BLINK;

      animPhase = PHASE_BLINK_CLOSE;

      blinkSpeed = 10;

      frameInterval = 1;

      draw_eyes(true);

      break;

    case 5: // Original blink(20)

      animKind = ANIM_BLINK;

      animPhase = PHASE_BLINK_CLOSE;

      blinkSpeed = 20;

      frameInterval = 1;

      draw_eyes(true);

      break;

    case 6: // Original happy_eye() with animated triangle offset

      center_eyes(false);

      animKind = ANIM_HAPPY;

      animPhase = PHASE_HAPPY_DRAW;

      happyOffset = ref_eye_height / 2;

      frameInterval = 1;

      break;

    case 7: // Original sleep()

      animKind = ANIM_SLEEP;

      left_eye_height = 2;

      right_eye_height = 2;

      draw_eyes(true);

      finishAnimation();

      break;

    case 8: // Original 20 random saccades out and back

      center_eyes(true);

      animKind = ANIM_SACCADES;

      animPhase = PHASE_SACCADE_OUT;

      frameInterval = 1;

      saccadesDone = 0;

      break;

  }

}

// =====================================================

// ORIGINAL MOVE-BIG-EYE, EXPRESSED AS FRAME STEPS

// Each original loop iteration drew one frame and delayed 1 ms.

// =====================================================

void applyMoveOutFrame() {

  int direction = (animKind == ANIM_MOVE_RIGHT) ? 1 : -1;

  const int direction_oversize = 1;

  const int direction_movement_amplitude = 2;

  const int blink_amplitude = 5;

  left_eye_x += direction_movement_amplitude * direction;

  right_eye_x += direction_movement_amplitude * direction;

  right_eye_height -= blink_amplitude;

  left_eye_height -= blink_amplitude;

  if (direction > 0) {

    right_eye_height += direction_oversize;

    right_eye_width += direction_oversize;

  } else {

    left_eye_height += direction_oversize;

    left_eye_width += direction_oversize;

  }

}

void applyMoveOutOpenFrame() {

  int direction = (animKind == ANIM_MOVE_RIGHT) ? 1 : -1;

  const int direction_oversize = 1;

  const int direction_movement_amplitude = 2;

  const int blink_amplitude = 5;

  left_eye_x += direction_movement_amplitude * direction;

  right_eye_x += direction_movement_amplitude * direction;

  right_eye_height += blink_amplitude;

  left_eye_height += blink_amplitude;

  if (direction > 0) {

    right_eye_height += direction_oversize;

    right_eye_width += direction_oversize;

  } else {

    left_eye_height += direction_oversize;

    left_eye_width += direction_oversize;

  }

}

void applyMoveBackCloseFrame() {

  int direction = (animKind == ANIM_MOVE_RIGHT) ? -1 : 1;

  const int direction_oversize = 1;

  const int direction_movement_amplitude = 2;

  const int blink_amplitude = 5;

  left_eye_x += direction_movement_amplitude * direction;

  right_eye_x += direction_movement_amplitude * direction;

  right_eye_height -= blink_amplitude;

  left_eye_height -= blink_amplitude;

  if (direction > 0) {

    right_eye_height -= direction_oversize;

    right_eye_width -= direction_oversize;

  } else {

    left_eye_height -= direction_oversize;

    left_eye_width -= direction_oversize;

  }

}

void applyMoveBackOpenFrame() {

  int direction = (animKind == ANIM_MOVE_RIGHT) ? -1 : 1;

  const int direction_oversize = 1;

  const int direction_movement_amplitude = 2;

  const int blink_amplitude = 5;

  left_eye_x += direction_movement_amplitude * direction;

  right_eye_x += direction_movement_amplitude * direction;

  right_eye_height += blink_amplitude;

  left_eye_height += blink_amplitude;

  if (direction > 0) {

    right_eye_height -= direction_oversize;

    right_eye_width -= direction_oversize;

  } else {

    left_eye_height -= direction_oversize;

    left_eye_width -= direction_oversize;

  }

}

// =====================================================

// ORIGINAL HAPPY EYE FRAME, WITH ORIGINAL TRIANGLE GEOMETRY

// =====================================================

void drawHappyEyeFrame() {

  if (!oledOK) return;

  int offset = happyOffset;

  display.fillTriangle(

    left_eye_x - left_eye_width / 2 - 1,

    left_eye_y + offset,

    left_eye_x + left_eye_width / 2 + 1,

    left_eye_y + 5 + offset,

    left_eye_x - left_eye_width / 2 - 1,

    left_eye_y + left_eye_height + offset,

    SSD1306_BLACK

  );

  display.fillTriangle(

    right_eye_x + right_eye_width / 2 + 1,

    right_eye_y + offset,

    right_eye_x - left_eye_width / 2 - 1,

    right_eye_y + 5 + offset,

    right_eye_x + right_eye_width / 2 + 1,

    right_eye_y + right_eye_height + offset,

    SSD1306_BLACK

  );

  display.display();

}

// =====================================================

// ONE ANIMATION STEP PER LOOP PASS; NO DELAY() IN ANIMATIONS

// =====================================================

void updateAnimation() {

  if (!animationRunning || birthdayMode || !oledOK) return;

  unsigned long now = millis();

  if (now - lastFrameAt < frameInterval) return;

  lastFrameAt = now;

  switch (animPhase) {

    case PHASE_WAKEUP_OPEN:

      // Original wakeup increments by 2 each frame, starting at 0.

      left_eye_height += 2;

      right_eye_height += 2;

      draw_eyes(true);

      if (left_eye_height >= ref_eye_height) {

        left_eye_height = right_eye_height = ref_eye_height;

        draw_eyes(true);

        finishAnimation();

      }

      break;

    case PHASE_BLINK_CLOSE:

      // Original blink: three steps down by speed.

      left_eye_height -= blinkSpeed;

      right_eye_height -= blinkSpeed;

      draw_eyes(true);

      phaseStep++;

      if (phaseStep >= 3) {

        phaseStep = 0;

        animPhase = PHASE_BLINK_OPEN;

      }

      break;

    case PHASE_BLINK_OPEN:

      // Original blink: three steps up by speed.

      left_eye_height += blinkSpeed;

      right_eye_height += blinkSpeed;

      draw_eyes(true);

      phaseStep++;

      if (phaseStep >= 3) {

        finishAnimation();

      }

      break;

    case PHASE_MOVE_OUT_CLOSE:

      applyMoveOutFrame();

      draw_eyes(true);

      phaseStep++;

      if (phaseStep >= 3) {

        phaseStep = 0;

        animPhase = PHASE_MOVE_OUT_OPEN;

      }

      break;

    case PHASE_MOVE_OUT_OPEN:

      applyMoveOutOpenFrame();

      draw_eyes(true);

      phaseStep++;

      if (phaseStep >= 3) {

        phaseStep = 0;

        animPhase = PHASE_MOVE_HOLD;

        phaseStartedAt = now;

      }

      break;

    case PHASE_MOVE_HOLD:

      // Original move_big_eye has delay(1000) between outward and return movement.

      if (now - phaseStartedAt >= 1000) {

        phaseStep = 0;

        animPhase = PHASE_MOVE_BACK_CLOSE;

      }

      break;

    case PHASE_MOVE_BACK_CLOSE:

      applyMoveBackCloseFrame();

      draw_eyes(true);

      phaseStep++;

      if (phaseStep >= 3) {

        phaseStep = 0;

        animPhase = PHASE_MOVE_BACK_OPEN;

      }

      break;

    case PHASE_MOVE_BACK_OPEN:

      applyMoveBackOpenFrame();

      draw_eyes(true);

      phaseStep++;

      if (phaseStep >= 3) {

        center_eyes(true);

        finishAnimation();

      }

      break;

    case PHASE_HAPPY_DRAW:

      // Original happy_eye starts with center_eyes(false), then offsets by -2.

      drawHappyEyeFrame();

      happyOffset -= 2;

      phaseStep++;

      if (phaseStep >= 10) {

        animPhase = PHASE_HAPPY_HOLD;

        phaseStartedAt = now;

      }

      break;

    case PHASE_HAPPY_HOLD:

      // Original happy_eye waits 1000 ms after drawing the triangles.

      if (now - phaseStartedAt >= 1000) {

        finishAnimation();

      }

      break;

    case PHASE_SACCADE_OUT: {

      // Exact original movement amplitudes: X=8, Y=6, height change=8.

      saccadeTargetX = random(-1, 2);

      saccadeTargetY = random(-1, 2);

      saccadeOriginXLeft = left_eye_x;

      saccadeOriginXRight = right_eye_x;

      saccadeOriginY = left_eye_y;

      left_eye_x += 8 * saccadeTargetX;

      right_eye_x += 8 * saccadeTargetX;

      left_eye_y += 6 * saccadeTargetY;

      right_eye_y += 6 * saccadeTargetY;

      right_eye_height -= 8;

      left_eye_height -= 8;

      draw_eyes(true);

      animPhase = PHASE_SACCADE_BACK;

      break;

    }

    case PHASE_SACCADE_BACK:

      // Opposite saccade returns to the exact prior position.

      left_eye_x = saccadeOriginXLeft;

      right_eye_x = saccadeOriginXRight;

      left_eye_y = right_eye_y = saccadeOriginY;

      right_eye_height += 8;

      left_eye_height += 8;

      draw_eyes(true);

      saccadesDone++;

      if (saccadesDone >= 20) {

        center_eyes(true);

        finishAnimation();

      } else {

        animPhase = PHASE_SACCADE_OUT;

      }

      break;

    case PHASE_NONE:

    default:

      finishAnimation();

      break;

  }

}

// =====================================================


// =====================================================
// BIRTHDAY MODE: ADDITIVE, MILLIS-DRIVEN SEQUENCE
// =====================================================
void enterBirthdayStage(BirthdayStage stage) {
  birthdayStage = stage;
  birthdayStageStarted = millis();
  birthdayFrameAt = 0;
  birthdayFrame = 0;

  if (stage == BD_SURPRISE) {
    center_eyes(false);
    left_eye_width = right_eye_width = 46;
    left_eye_height = right_eye_height = 46;
    draw_eyes(true);
  } else if (stage == BD_HAPPY) {
    center_eyes(false);
    draw_eyes(false);
    happyOffset = 0;
    drawHappyEyeFrame();
  } else if (stage == BD_HEART) {
    startBirthdayMelody();
    drawHeartScene(0);
  } else if (stage == BD_CAKE) {
    drawCakeScene(0);
  } else if (stage == BD_MESSAGES) {
    messageScreen = 0;
    showBirthdayMessage();
  }
}

void drawPixelHeart(int cx, int cy, int scale) {
  display.fillCircle(cx - scale/2, cy - scale/3, scale/3, SSD1306_WHITE);
  display.fillCircle(cx + scale/2, cy - scale/3, scale/3, SSD1306_WHITE);
  display.fillTriangle(cx-scale, cy-scale/6, cx+scale, cy-scale/6,
                       cx, cy+scale, SSD1306_WHITE);
}

void drawHeartScene(int frame) {
  if (!oledOK) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(34, 4);
  display.print(F("A LITTLE LOVE"));
  drawPixelHeart(64, 34, (frame % 2 == 0) ? 13 : 16);
  if ((frame % 3) != 1) {
    display.drawPixel(35, 24, SSD1306_WHITE);
    display.drawPixel(29, 34, SSD1306_WHITE);
    display.drawPixel(92, 22, SSD1306_WHITE);
    display.drawPixel(99, 39, SSD1306_WHITE);
  }
  if ((frame % 3) == 0) {
    display.drawLine(43, 17, 43, 21, SSD1306_WHITE);
    display.drawLine(41, 19, 45, 19, SSD1306_WHITE);
    display.drawLine(84, 43, 84, 47, SSD1306_WHITE);
    display.drawLine(82, 45, 86, 45, SSD1306_WHITE);
  }
  display.display();
}

void drawCakeScene(int frame) {
  if (!oledOK) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(39, 2);
  display.print(F("MAKE A WISH!"));
  display.drawRect(39, 34, 50, 18, SSD1306_WHITE);
  display.drawLine(39, 40, 88, 40, SSD1306_WHITE);
  display.drawLine(39, 46, 88, 46, SSD1306_WHITE);
  for (int x = 43; x <= 85; x += 6) {
    display.drawPixel(x, 35, SSD1306_WHITE);
    display.drawPixel(x, 37, SSD1306_WHITE);
  }
  for (int x = 49; x <= 79; x += 15) {
    display.drawLine(x, 34, x, 25, SSD1306_WHITE);
    if ((frame + x) % 2 == 0)
      display.fillTriangle(x-3, 25, x+3, 25, x, 19, SSD1306_WHITE);
    else
      display.fillCircle(x, 23, 2, SSD1306_WHITE);
  }
  if (frame % 2 == 0) {
    display.drawLine(20, 18, 20, 24, SSD1306_WHITE);
    display.drawLine(17, 21, 23, 21, SSD1306_WHITE);
    display.drawLine(105, 17, 105, 23, SSD1306_WHITE);
    display.drawLine(102, 20, 108, 20, SSD1306_WHITE);
  } else {
    display.drawPixel(18, 17, SSD1306_WHITE);
    display.drawPixel(24, 27, SSD1306_WHITE);
    display.drawPixel(103, 27, SSD1306_WHITE);
    display.drawPixel(111, 15, SSD1306_WHITE);
  }
  display.display();
}

// Persistent first message screen: greeting plus requested subheading.
void showBirthdayMessage() {
  if (!oledOK) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(10, 7); display.println(F("HAPPY"));
  display.setCursor(10, 27); display.println(F("BIRTHDAY!"));
  display.setTextSize(1);
  display.setCursor(8, 53); display.print(F("WISHING YOU THE BEST"));
  display.display();
}

void showMessageScreen(int screen) {
  if (!oledOK) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  switch (screen) {
    case 1:
      display.setCursor(13, 13); display.println(F("I MAY BE SMALL..."));
      display.setCursor(8, 32); display.println(F("BUT MY HEART IS BIG"));
      drawPixelHeart(111, 47, 7);
      break;
    case 2:
      display.setCursor(28, 14); display.println(F("THANK YOU FOR"));
      display.setCursor(22, 34); display.println(F("BEING AWESOME!"));
      break;
    case 3:
      display.setCursor(43, 8); display.println(F("PSSST..."));
      display.setCursor(25, 25); display.println(F("I HAVE A SECRET."));
      display.setCursor(31, 44); display.println(F("PRESS AGAIN..."));
      break;
    case 4:
      display.setCursor(25, 8); display.println(F("BY THE WAY..."));
      display.setCursor(13, 27); display.println(F("FEEL FREE TO"));
      display.setCursor(19, 46); display.println(F("REPROGRAM ME"));
      break;
    case 5:
      display.setCursor(10, 8); display.println(F("TO LEARN AND GET"));
      display.setCursor(16, 25); display.println(F("INTO ROBOTICS"));
      display.setCursor(22, 43); display.println(F("& MICROCONTROLLERS"));
      break;
    default:
      display.setCursor(7, 7); display.println(F("MAKE, CODE, EXPLORE!"));
      display.setCursor(12, 25); display.println(F("LEARN ROBOTICS"));
      display.setCursor(15, 43); display.println(F("AND HAVE FUN!"));
      break;
  }
  display.display();
}

void handleBirthdayButtonPress() {
  // Ignore presses during automatic animation; only message screens advance.
  if (birthdayStage != BD_MESSAGES) return;
  messageScreen++;
  if (messageScreen <= 5) {
    showMessageScreen(messageScreen);
  } else {
    stopBirthdayMelody();
    birthdayMode = false;
    birthdayStage = BD_IDLE;
    animationRunning = false;
    animKind = ANIM_NONE;
    animPhase = PHASE_NONE;
    manualAnimation = false;
    center_eyes(true);
    current_animation_index = 0;
    demo_mode = 1;
    startupStage = 0;
    Serial.println(F("Birthday mode OFF"));
  }
}

void updateBirthdaySequence() {
  if (!birthdayMode || !oledOK) return;
  unsigned long now = millis();
  switch (birthdayStage) {
    case BD_SURPRISE:
      if (now - birthdayStageStarted >= 900) enterBirthdayStage(BD_HAPPY);
      break;
    case BD_HAPPY:
      if (now - birthdayStageStarted >= 850) enterBirthdayStage(BD_HEART);
      break;
    case BD_HEART:
      if (now - birthdayFrameAt >= 220) {
        birthdayFrameAt = now;
        drawHeartScene(++birthdayFrame);
      }
      if (now - birthdayStageStarted >= 5200) enterBirthdayStage(BD_CAKE);
      break;
    case BD_CAKE:
      if (now - birthdayFrameAt >= 260) {
        birthdayFrameAt = now;
        drawCakeScene(++birthdayFrame);
      }
      if (now - birthdayStageStarted >= 13200) {
        stopBirthdayMelody();
        enterBirthdayStage(BD_MESSAGES);
      }
      break;
    case BD_MESSAGES:
    case BD_IDLE:
    default:
      break;
  }
}

// SERIAL COMMANDS

// I = I2C test, R = recover OLED, A0..A8 = play animation

// =====================================================

void processSerialCommands() {

  static bool readingAnimationNumber = false;

  static int animationNumber = 0;

  static bool haveDigit = false;

  while (Serial.available() > 0) {

    char c = Serial.read();

    if (readingAnimationNumber) {

      if (c >= '0' && c <= '9') {

        animationNumber = animationNumber * 10 + (c - '0');

        haveDigit = true;

      } else {

        // A newline or other separator finishes the command.

        if (haveDigit && !birthdayMode) {

          startAnimation(animationNumber, true);

          Serial.print(F("Manual animation: "));

          Serial.println(animationNumber);

        }

        readingAnimationNumber = false;

        animationNumber = 0;

        haveDigit = false;

        // Handle a command character that ended the number.

        if (c == 'i' || c == 'I') checkI2C();

        else if (c == 'r' || c == 'R') restartOLED();

      }

      continue;

    }

    if (c == 'i' || c == 'I') {

      checkI2C();

    } else if (c == 'r' || c == 'R') {

      restartOLED();

    } else if (c == 'A' || c == 'a') {

      readingAnimationNumber = true;

      animationNumber = 0;

      haveDigit = false;

    }

  }

}

// =====================================================

// SETUP / LOOP

// =====================================================

void setup() {

  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);

  noTone(BUZZER_PIN);

  Wire.begin();

  Wire.setClock(100000);

  randomSeed(analogRead(A0));

  Serial.println(F("\n=== ORIGINAL-STYLE NON-BLOCKING EYES ==="));

  Serial.println(F("Button D3 -> GND | OLED SDA=A4, SCL=A5 | address 0x3C"));

  Serial.println(F("I = I2C check | R = OLED recovery | A0..A8 = animation"));

  oledOK = display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);

  if (!oledOK) {

    Serial.println(F("OLED initialization FAILED"));

    return;

  }

  Serial.println(F("OLED initialized"));

  display.clearDisplay();

  display.setTextSize(1);

  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);

  display.println(F("Intellar.ca"));

  display.display();

  rawButtonState = stableButtonState = digitalRead(BUTTON_PIN);

  lastButtonChange = millis();

  // Preserve original startup timing without blocking button polling.

  startupStage = 1;  // splash is already displayed

  startupStageStarted = millis();

  phaseStartedAt = millis();

  animKind = ANIM_NONE;

  animationRunning = false;

  current_animation_index = 0;

}

void loop() {
  // Keep the existing debounced button polling responsive.
  if (buttonPressed()) {
    pressCount++;
    Serial.print(F("Button press #"));
    Serial.println(pressCount);

    if (!birthdayMode) {
      birthdayMode = true;
      animationRunning = false;
      animKind = ANIM_NONE;
      animPhase = PHASE_NONE;
      manualAnimation = false;
      stopBirthdayMelody();
      enterBirthdayStage(BD_SURPRISE);
      Serial.println(F("Birthday sequence started"));
    } else {
      handleBirthdayButtonPress();
    }
  }

  processSerialCommands();
  updateBirthdayMelody();

  if (birthdayMode) {
    updateBirthdaySequence();
    return;
  }
  if (!oledOK) return;

  // Original startup timing: splash 2 s, sleeping eyes 2 s, then demo.
  if (startupStage == 1) {
    if (millis() - startupStageStarted >= 2000) {
      left_eye_height = 2;
      right_eye_height = 2;
      draw_eyes(true);
      startupStage = 2;
      startupStageStarted = millis();
    }
    return;
  }

  if (startupStage == 2) {
    if (millis() - startupStageStarted >= 2000) {
      startupStage = 0;
      center_eyes(true);
    }
    return;
  }

  if (!animationRunning && demo_mode == 1) {
    startAnimation(current_animation_index, false);
  }
  updateAnimation();
}

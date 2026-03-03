# dash_buttons.py - Touch button panel for sub-screen (HDMI1 or window)
# Sends MQTT commands to front/rear ESP32 modules
# Run with: python3 dash_buttons.py
# Designed for touchscreens (7", 10", 12.3" etc.)

import pygame
import paho.mqtt.client as mqtt
import time
import sys

# --------------------- MQTT Config ---------------------
MQTT_BROKER = "localhost"          # or "127.0.0.1" if Mosquitto runs on same Pi
MQTT_PORT   = 1883
MQTT_CLIENT_ID = "DashButtons"

client = mqtt.Client(MQTT_CLIENT_ID)
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_start()

# --------------------- Screen & Colors ---------------------
# Change these based on your sub-screen resolution
WIDTH, HEIGHT = 1280, 800           # Example for 10.1" or 12.3" landscape
BUTTON_W = 220
BUTTON_H = 100
GAP = 20

BLACK   = (0, 0, 0)
WHITE   = (255, 255, 255)
GRAY    = (80, 80, 80)
GREEN   = (0, 180, 0)
RED     = (220, 0, 0)
BLUE    = (0, 120, 220)
YELLOW  = (220, 220, 0)
ORANGE  = (255, 140, 0)

pygame.init()
pygame.font.init()
screen = pygame.display.set_mode((WIDTH, HEIGHT), pygame.FULLSCREEN)  # or remove FULLSCREEN for window testing
font_large = pygame.font.SysFont('arial', 36, bold=True)
font_small = pygame.font.SysFont('arial', 24)

clock = pygame.time.Clock()

# --------------------- Button Definitions ---------------------
# Format: (x, y, width, height, label, bg_color, text_color, topic, payload_on, payload_off, toggle=True/False)
buttons = [

    # Lighting group
    (GAP, GAP,          BUTTON_W, BUTTON_H, "Headlamps",      GREEN, WHITE, "lights/head_on",      "1", "0", True),
    (GAP*2 + BUTTON_W, GAP,     BUTTON_W, BUTTON_H, "Highbeam",      BLUE,  WHITE, "lights/highbeam",     "1", "0", True),
    (GAP*3 + BUTTON_W*2, GAP,   BUTTON_W, BUTTON_H, "Halo Lights",    YELLOW, BLACK, "lights/halo",         "1", "0", True),
    (GAP*4 + BUTTON_W*3, GAP,   BUTTON_W, BUTTON_H, "Parking Lamps",  ORANGE, BLACK, "lights/parking",      "1", "0", True),

    # Hazard & Patterns
    (GAP, GAP*2 + BUTTON_H,     BUTTON_W*2 + GAP, BUTTON_H, "HAZARD", RED, WHITE, "lights/hazard",       "1", "0", True),

    # Rear pattern selector (simple buttons 1-5)
    (GAP*3 + BUTTON_W*2, GAP*2 + BUTTON_H, BUTTON_W//2, BUTTON_H, "P1", GRAY, WHITE, "rear/pattern", "1", None, False),
    (GAP*3 + BUTTON_W*2.5 + GAP//2, GAP*2 + BUTTON_H, BUTTON_W//2, BUTTON_H, "P2", GRAY, WHITE, "rear/pattern", "2", None, False),
    (GAP*3 + BUTTON_W*3 + GAP, GAP*2 + BUTTON_H, BUTTON_W//2, BUTTON_H, "P3", GRAY, WHITE, "rear/pattern", "3", None, False),
    (GAP*3 + BUTTON_W*3.5 + GAP*1.5, GAP*2 + BUTTON_H, BUTTON_W//2, BUTTON_H, "P4", GRAY, WHITE, "rear/pattern", "4", None, False),
    (GAP*3 + BUTTON_W*4 + GAP*2, GAP*2 + BUTTON_H, BUTTON_W//2, BUTTON_H, "P5", GRAY, WHITE, "rear/pattern", "5", None, False),

    # Engine controls
    (GAP, GAP*3 + BUTTON_H*2,   BUTTON_W, BUTTON_H, "Start Engine",  GREEN, WHITE, "engine/start",        "1", None, False),
    (GAP*2 + BUTTON_W, GAP*3 + BUTTON_H*2, BUTTON_W, BUTTON_H, "Stop Engine",   RED,   WHITE, "engine/stop",         "1", None, False),
    (GAP*3 + BUTTON_W*2, GAP*3 + BUTTON_H*2, BUTTON_W, BUTTON_H, "Fuel Pump",    BLUE,  WHITE, "engine/fuel_manual",  "1", "0", True),
    (GAP*4 + BUTTON_W*3, GAP*3 + BUTTON_H*2, BUTTON_W, BUTTON_H, "Fan Override",  ORANGE, BLACK, "engine/fan_override", "1", "0", True),
    (GAP*5 + BUTTON_W*4, GAP*3 + BUTTON_H*2, BUTTON_W, BUTTON_H, "Water Pump",    BLUE,  WHITE, "engine/water_manual", "1", "0", True),

    # Wipers & Heat (blower)
    (GAP, GAP*4 + BUTTON_H*3,   BUTTON_W, BUTTON_H, "Wipers OFF",    GRAY, WHITE, "wipers/speed", "0", None, False),
    (GAP*2 + BUTTON_W, GAP*4 + BUTTON_H*3, BUTTON_W, BUTTON_H, "Wipers LOW",    BLUE, WHITE, "wipers/speed", "1", None, False),
    (GAP*3 + BUTTON_W*2, GAP*4 + BUTTON_H*3, BUTTON_W, BUTTON_H, "Wipers HIGH",   BLUE, WHITE, "wipers/speed", "2", None, False),
]

# Track toggle states (for buttons that are on/off)
toggle_states = {btn[6]: False for btn in buttons if btn[9] is not None}  # topic → current state

# --------------------- Draw Button ---------------------
def draw_button(x, y, w, h, text, bg, fg):
    pygame.draw.rect(screen, bg, (x, y, w, h), border_radius=12)
    pygame.draw.rect(screen, WHITE, (x, y, w, h), 3, border_radius=12)  # border
    label = font_large.render(text, True, fg)
    screen.blit(label, (x + (w - label.get_width()) // 2, y + (h - label.get_height()) // 2))

# --------------------- Main Loop ---------------------
running = True
while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
        elif event.type == pygame.MOUSEBUTTONDOWN:
            mx, my = event.pos
            for btn in buttons:
                x, y, w, h, label, bg, fg, topic, on_payload, off_payload, is_toggle = btn
                if x <= mx <= x + w and y <= my <= y + h:
                    if is_toggle:
                        # Toggle state
                        toggle_states[topic] = not toggle_states[topic]
                        payload = on_payload if toggle_states[topic] else off_payload
                        client.publish(topic, payload)
                        print(f"Toggle {label} → {payload}")
                    else:
                        # Momentary / one-shot
                        client.publish(topic, on_payload)
                        print(f"Pressed {label} → {on_payload}")

    screen.fill(BLACK)

    # Draw all buttons
    for btn in buttons:
        x, y, w, h, label, bg, fg, topic, _, _, is_toggle = btn

        # Highlight active toggles
        if is_toggle and toggle_states.get(topic, False):
            bg = (bg[0]+40, bg[1]+40, bg[2]+40)  # brighter when ON

        draw_button(x, y, w, h, label, bg, fg)

    pygame.display.flip()
    clock.tick(30)

client.loop_stop()
client.disconnect()
pygame.quit()
sys.exit()

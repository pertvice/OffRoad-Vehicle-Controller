# merged_dash.py - Combined Gauges + Touch Buttons for single-screen testing
# Optimized for 1024×600 7-inch touchscreen on Pi 3B+

import pygame
import paho.mqtt.client as mqtt
import configparser
import json
import time
import math

# Load configuration
config = configparser.ConfigParser()
config.read('dashboard.ini')

WIDTH = int(config.get('screen', 'width', fallback='1024'))
HEIGHT = int(config.get('screen', 'height', fallback='600'))

# Load gauge layout (or use fallback if ini missing)
try:
    GAUGE_LAYOUT = json.loads(config.get('gauges', 'layout_json'))
except:
    GAUGE_LAYOUT = [
        {"type":"analog","x":100,"y":120,"label":"Tach","min":0,"max":8000,"sensor":"rpm","unit":"RPM"},
        {"type":"analog","x":320,"y":120,"label":"Speed","min":0,"max":160,"sensor":"speed","unit":"MPH"},
        {"type":"analog","x":540,"y":120,"label":"Fuel P","min":0,"max":100,"sensor":"fuel_pressure","unit":"PSI"},
        {"type":"analog","x":760,"y":120,"label":"Oil P","min":0,"max":150,"sensor":"oil_pressure","unit":"PSI"}
    ]

pygame.init()
screen = pygame.display.set_mode((WIDTH, HEIGHT), pygame.FULLSCREEN)
pygame.font.init()

font = pygame.font.SysFont('arial', 24)
bigfont = pygame.font.SysFont('arial', 36, bold=True)

# MQTT setup
client = mqtt.Client()
client.connect("localhost", 1883, 60)
client.loop_start()

# Shared sensor/indicator values
values = {
    "rpm": 0, "speed": 0, "fuel_pressure": 0, "oil_pressure": 0,
    "highbeam": 0, "hazard": 0
}

toggle_states = {}

# Button layout - right sidebar
BUTTON_W = 160
BUTTON_H = 60
GAP = 8

buttons = [
    (WIDTH - BUTTON_W - GAP, GAP, BUTTON_W, BUTTON_H, "Headlamps", (0, 180, 0), (255, 255, 255), "lights/head_on", "1", "0", True),
    (WIDTH - BUTTON_W - GAP, GAP + BUTTON_H + GAP, BUTTON_W, BUTTON_H, "Highbeam", (0, 120, 220), (255, 255, 255), "lights/highbeam", "1", "0", True),
    (WIDTH - BUTTON_W - GAP, GAP + (BUTTON_H + GAP)*2, BUTTON_W, BUTTON_H, "Halo", (220, 220, 0), (0, 0, 0), "lights/halo", "1", "0", True),
    (WIDTH - BUTTON_W - GAP, GAP + (BUTTON_H + GAP)*3, BUTTON_W, BUTTON_H, "Parking", (255, 140, 0), (0, 0, 0), "lights/parking", "1", "0", True),
    (WIDTH - BUTTON_W*2 - GAP*2, GAP, BUTTON_W, BUTTON_H, "Hazard", (220, 0, 0), (255, 255, 255), "lights/hazard", "1", "0", True),
    (WIDTH - BUTTON_W*2 - GAP*2, GAP + BUTTON_H + GAP, BUTTON_W, BUTTON_H, "Fuel Pump", (0, 120, 220), (255, 255, 255), "engine/fuel_manual", "1", "0", True),
]

# Initialize toggle states
for btn in buttons:
    _, _, _, _, _, _, _, topic, _, _, is_toggle = btn
    if is_toggle:
        toggle_states[topic] = False

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        values.update(data)
        print("Received MQTT:", data)  # Debug: see incoming sensor data
    except Exception as e:
        print("MQTT parse error:", e)

client.on_message = on_message
client.subscribe("sensors/#")

def draw_analog_gauge(x, y, val, minv, maxv, label, unit=""):
    radius = 60
    center = (x, y)
    pygame.draw.circle(screen, (40, 40, 40), center, radius, 5)
    angle = 135 + ((val - minv) / (maxv - minv)) * 270
    ex = center[0] + radius * 0.75 * math.cos(math.radians(angle))
    ey = center[1] - radius * 0.75 * math.sin(math.radians(angle))
    pygame.draw.line(screen, (0, 255, 100), center, (ex, ey), 5)
    txt = font.render(f"{label} {val:.0f}{unit}", True, (220, 220, 220))
    screen.blit(txt, (x - txt.get_width()//2, y + radius + 5))

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
                        toggle_states[topic] = not toggle_states[topic]
                        payload = on_payload if toggle_states[topic] else off_payload
                        client.publish(topic, payload)
                        print(f"Button '{label}' toggled → MQTT sent: {topic} = {payload}")
                    else:
                        client.publish(topic, on_payload)
                        print(f"Button '{label}' pressed → MQTT sent: {topic} = {on_payload}")

    screen.fill((10, 10, 20))

    # Draw gauges on left
    for g in GAUGE_LAYOUT:
        v = values.get(g["sensor"], 0)
        if g["type"] == "analog":
            draw_analog_gauge(g["x"], g["y"], v, g["min"], g["max"], g["label"], g.get("unit", ""))

    # Draw buttons on right
    for btn in buttons:
        x, y, w, h, label, bg, fg, topic, _, _, is_toggle = btn
        current_bg = tuple(min(255, c + 60) for c in bg) if toggle_states.get(topic, False) else bg
        pygame.draw.rect(screen, current_bg, (x, y, w, h), border_radius=8)
        pygame.draw.rect(screen, (255, 255, 255), (x, y, w, h), 2, border_radius=8)
        txt = font.render(label, True, fg)
        screen.blit(txt, (x + (w - txt.get_width())//2, y + (h - txt.get_height())//2))

    pygame.display.flip()
    time.sleep(0.03)

client.loop_stop()
pygame.quit()

import pygame
import paho.mqtt.client as mqtt
import configparser
import json
import time
import math

config = configparser.ConfigParser()
config.read('dashboard.ini')

WIDTH = int(config['screen']['width'])
HEIGHT = int(config['screen']['height'])
GAUGE_LAYOUT = json.loads(config['gauges']['layout_json'])

pygame.init()
screen = pygame.display.set_mode((WIDTH, HEIGHT), pygame.FULLSCREEN)

client = mqtt.Client()
client.connect("localhost", 1883, 60)
client.loop_start()

def draw_analog_gauge(surface, x, y, value, min_val, max_val, label, unit="", color=(0,255,0)):
    radius = 80
    center = (x, y)
    pygame.draw.circle(surface, (60,60,60), center, radius, 4)
    angle = 135 + (value - min_val) / (max_val - min_val) * 270
    end_x = center[0] + radius * 0.85 * math.cos(math.radians(angle))
    end_y = center[1] - radius * 0.85 * math.sin(math.radians(angle))
    pygame.draw.line(surface, color, center, (end_x, end_y), 6)
    font = pygame.font.SysFont('arial', 24)
    text = font.render(f"{label}: {value:.1f}{unit}", True, (220,220,220))
    surface.blit(text, (x - text.get_width()//2, y + radius + 10))

running = True
values = {"rpm": 0, "fuel_pressure": 0, "oil_pressure": 0}  # Update via MQTT

while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
        # Add touch button detection here

    screen.fill((20,20,30))
    for g in GAUGE_LAYOUT:
        val = values.get(g["sensor"], 0)
        draw_analog_gauge(screen, g["x"], g["y"], val, g["min"], g["max"], g["label"], g.get("unit",""))

    pygame.display.flip()
    time.sleep(0.02)

client.loop_stop()
pygame.quit()

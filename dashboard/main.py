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
font = pygame.font.SysFont('arial', 28)
bigfont = pygame.font.SysFont('arial', 48, bold=True)

client = mqtt.Client()
client.connect("localhost", 1883, 60)
client.loop_start()

values = {"rpm":0, "speed":0, "fuel_pressure":0, "oil_pressure":0, "boost":0,
          "coolant_temp":0, "trans_temp":0, "oil_temp":0, "battery_volt":12.6,
          "fuel_level":50, "highbeam":0, "turn_l":0, "turn_r":0, "hazard":0}

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload)
        values.update(data)
    except:
        pass

client.on_message = on_message
client.subscribe("sensors/#")

def draw_analog_gauge(x, y, val, minv, maxv, label, unit):
    radius = 85
    center = (x, y)
    pygame.draw.circle(screen, (40,40,40), center, radius, 8)
    angle = 135 + ((val - minv) / (maxv - minv)) * 270
    ex = center[0] + radius * 0.8 * math.cos(math.radians(angle))
    ey = center[1] - radius * 0.8 * math.sin(math.radians(angle))
    pygame.draw.line(screen, (0,255,100), center, (ex, ey), 8)
    txt = font.render(f"{label} {val:.0f}{unit}", True, (220,220,220))
    screen.blit(txt, (x - txt.get_width()//2, y + radius + 15))

running = True
while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

    screen.fill((10,10,20))

    for g in GAUGE_LAYOUT:
        v = values.get(g["sensor"], 0)
        if g["type"] == "analog":
            draw_analog_gauge(g["x"], g["y"], v, g["min"], g["max"], g["label"], g.get("unit",""))
        elif g["type"] == "digital":
            txt = bigfont.render(f"{g['label']}: {v}{g.get('unit','')}", True, (0,255,200))
            screen.blit(txt, (g["x"], g["y"]))
        elif g["type"] == "indicator":
            color = (0,255,0) if values.get(g["label"].lower().replace(" ","_"), 0) else (100,100,100)
            pygame.draw.circle(screen, color, (g["x"], g["y"]), 25)

    pygame.display.flip()
    time.sleep(0.016)

client.loop_stop()
pygame.quit()

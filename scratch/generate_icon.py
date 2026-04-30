from PIL import Image, ImageDraw, ImageFont
import os

# Create a 512x512 transparent image
size = 512
img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
draw = ImageDraw.Draw(img)

# Blue rounded rectangle settings
rect_color = (43, 108, 246) # Tailwind blue-500-ish vibrant blue
radius = 100
padding = 10

# Draw rounded rectangle
draw.rounded_rectangle(
    [(padding, padding), (size - padding, size - padding)],
    radius=radius,
    fill=rect_color
)

# Text "MF" settings
text = "MF"
text_color = (255, 255, 255)

# Try to load a bold font
try:
    font = ImageFont.truetype("segoeuib.ttf", 240)
except IOError:
    try:
        font = ImageFont.truetype("arialbd.ttf", 240)
    except IOError:
        font = ImageFont.load_default()

# Get text bounding box to center it
bbox = draw.textbbox((0, 0), text, font=font)
text_width = bbox[2] - bbox[0]
text_height = bbox[3] - bbox[1]

# Calculate position
x = (size - text_width) / 2
# Fine-tune vertical center for uppercase letters
y = (size - text_height) / 2 - 30 

draw.text((x, y), text, fill=text_color, font=font)

# Ensure the build directory exists
os.makedirs("build", exist_ok=True)

# Save to build/icon.png
img.save("build/icon.png", format="PNG")
print("Icon successfully generated at build/icon.png")

import os
from PIL import Image

# Configuration
input_folder = "source_frames"  # Folder containing your original PNGs/JPGs
output_folder = "data"          # The 'data' folder for LittleFS upload
# target_size has to match the panel size used in config.h (SCREEN_ROTATION):
# 320x240 for the ILI9341 in landscape. Change it when the TFT is swapped.
target_size = (320, 240)

os.makedirs(output_folder, exist_ok=True)
os.makedirs(input_folder, exist_ok=True)

for filename in sorted(os.listdir(input_folder)):
    if filename.lower().endswith(('.png', '.jpg', '.jpeg')):
        img_path = os.path.join(input_folder, filename)
        img = Image.open(img_path).convert("RGB")
        
        # Resize if it doesn't match the target dimensions
        if img.size != target_size:
            img = img.resize(target_size, Image.Resampling.LANCZOS)
            
        raw_data = bytearray()
        for pixel in img.getdata():
            r, g, b = pixel
            # Convert RGB888 to RGB565
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            # Append bytes (little-endian order to match standard ESP32 expectations)
            raw_data.append(rgb565 & 0xFF)        # Low byte
            chno = (rgb565 >> 8) & 0xFF           # High byte
            raw_data.append(chno)

        # Save as .raw file with the same base name
        base_name = os.path.splitext(filename)[0]
        output_filename = f"{base_name}.raw"
        output_path = os.path.join(output_folder, output_filename)
        
        with open(output_path, "wb") as f:
            f.write(raw_data)
            
        print(f"Converted: {filename} -> {output_filename}")

print("All frames converted successfully!")
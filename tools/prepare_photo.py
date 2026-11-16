from PIL import Image, ImageOps
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'firmware/esp32-hmi/assets'
im=ImageOps.fit(Image.open(p/'lathe-illustration.png').convert('RGB'),(198,194),method=Image.Resampling.LANCZOS)
data=[]
for r,g,b in im.getdata():
 v=((r>>3)<<11)|((g>>2)<<5)|(b>>3);data.extend([v&255,v>>8])
with (p/'tv4_lathe.c').open('w') as f:
 f.write('/* Generated illustrative still. See IMAGE-PROVENANCE.md. */\n#include "lvgl.h"\nstatic const uint8_t pixels[]={\n')
 for i in range(0,len(data),32):f.write(','.join(map(str,data[i:i+32]))+',\n')
 f.write('};\nconst lv_image_dsc_t tv4_lathe={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=198,.h=194,.stride=396},.data_size=sizeof(pixels),.data=pixels};\n')

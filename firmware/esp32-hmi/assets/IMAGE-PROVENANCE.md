# Иллюстрация обработки

`lathe-illustration.png` создана встроенным инструментом image_gen; CLI/API fallback не использовался. Это декоративная иллюстрация, не камера и не состояние станка.
`tv4_lathe.c` — производный RGB565-ресурс 198×194, 76 824 байта пикселей; RGB565 уменьшает flash/память относительно ARGB8888. Исходный PNG не компилируется в прошивку.

## Полный промпт

Generate a photorealistic industrial lathe turning closeup for a CNC touchscreen dashboard. Landscape 4:3 composition. A large dark polished steel three-jaw chuck fills the left side, its face pointing to the right; a shiny short stepped cylindrical metal workpiece extends rightwards from the chuck. A black square metal tool post with stacked bolts occupies the far right edge, a small gold carbide insert cuts the workpiece near lower right, curled shiny swarf below. Dark blue-black workshop background, cinematic cool steel highlights and restrained warm reflection at cutting tip, realistic machined metal rings, luxurious sharp technical product photography. Tight crop, no people, no text, no UI, no borders, no watermark. This is an illustrative still image, not telemetry.

## Значки

Значки — оригинальные процедурные контуры, не сгенерированный интерфейс. Источник: `tools/generate_icons.py`. Экран, типографика, границы, кнопки, градиенты и показания создаются объектами LVGL.

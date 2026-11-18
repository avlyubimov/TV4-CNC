# Проверки проекта

Актуальный снимок: [PR01_REVIEW.md](PR01_REVIEW.md),
[HARDWARE_STATUS.md](HARDWARE_STATUS.md), [IMPLEMENTATION.md](IMPLEMENTATION.md).
Историческая SDK-матрица: [IDF_611_COMPATIBILITY.md](IDF_611_COMPATIBILITY.md).

main 323ab36: docs, host/sanitizers и ESP-IDF 6.1.1/5.3.2 × headless/rev12
успешны в Actions. В текущем main пять host tests; старые отчёты 4/4 относятся
к предшествующему этапу. PR #5 проверен отдельно и не включён в main.
Результаты ссылками зафиксированы в отчёте PR-01; это не новые измерения платы.

Заводской backup 16 МиБ сохранён/проверен, restore не испытан.
Фото подтверждает вывод HMI/списка Wi-Fi, а не полное испытание сенсора,
подключения, FPS/latency или motion. Кадры [UI_REFERENCE.md](UI_REFERENCE.md)
и host render не считать фотографиями аппаратных измерений.
PR-01 не подключает и не прошивает устройства, не пересобирает прошивку локально.

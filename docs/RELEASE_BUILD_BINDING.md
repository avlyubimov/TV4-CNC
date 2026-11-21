# PR-05d — offline-связь build candidate и signed cohort

База main `aa06833`, PR #1–40 слиты. [План](plans/PR05D_RELEASE_BUILD_BINDING.md),
[проверки](evidence/build/release-build-binding/README.md). Устройств этап не касается.

## Что проверяется

[review_release_build.py](../tools/review_release_build.py) повторно проверяет
существующий [unsigned build candidate](BUILD_CANDIDATES.md) и
[signed HMI/optional Motion cohort](RELEASE_COHORT_REVIEW.md). Это связь конкретных
локальных bytes и утверждений metadata, не доказательство происхождения сборки.
Новые production manifest/schema, signing service и runtime caller не добавлены.

Обязательные дополнительные local inputs: директория пяти файлов candidate,
отдельно выбранный SHA-256 **точных candidate.json bytes**, SDK/profile и ELF.
Expected source SHA берётся из независимого review, не downloaded metadata.
Pin выбирается вне загруженного комплекта. Вычисление pin из того же неизвестного
комплекта проверит только его целостность; не установит CI identity или автора.

Проверяются строгая schema/canonical JSON/inventory и прежние candidate gates,
expected source/SDK/profile, весь HMI app image и descriptor, SHA/length локального
ELF. Затем прежние HMI/OpenSSL и independent Motion/Mbed TLS verifier'ы проверяют
подписи/версии/context/полные образы/prospective peer range. Candidate application
полностью сравнивается с inspected app того же signed cohort, включая ELF digest.
ELF **не исполняется и не разбирается как executable**: только digest/length;
`elf_format_checked=false`. Source tree/config/partition/dependency/license hashes
остаются captured unsigned claims, не независимая перепроверка всех build inputs.

## Bytes, границы и совместимость

- Все пять фиксированных candidate filenames, HMI image и ELF читаются как
  bounded regular inputs и помещаются в private snapshot. Документ не выбирает
  executable, команду, URL или путь. Symlink, включая parents, FIFO/nonregular,
  empty/oversize, missing/extra candidate files отклоняются.
- Candidate pin проверяется до разбора candidate JSON и повторно связывается с
  captured validation result. Metadata не перечитывается после проверки для
  получения binding fields. Замена исходного image/ELF после snapshot не меняет
  результат; разные valid images между двумя verifier'ами не совпадут.
- Candidate file≤65536 bytes, HMI image≤2MiB, ELF≤32MiB — host tooling ceilings,
  не RAM/Flash capacity F407 или approved runtime memory policy. Прежние cohort
  limits, trusted local toolchain и quiescent owned FS precondition сохраняются.
  Инструмент не sandbox для hostile executable/local filesystem actor.
- `build_candidate.verify_with_metadata()` и `cohort.review_with_details()`
  возвращают captured checked fields для внутреннего read-only audit.
  Старые `verify()`/`review()` и CLI JSON reports сохранены. В build verify есть
  optional `--candidate-sha256`; это pin unsigned bytes, не authentication.
  HMI internal metadata дополнена signed profile/layout, без нового policy API.

## Factory и release blockers

Разметка candidate по-прежнему `factory-single-2m`, без OTA slots. Signed HMI
policy требует `tv4-16m-ab-v1`. Audit явно выдаёт `hmi-build-layout-mismatch`;
совпадение image SHA не превращает factory build в OTA recovery/A-B image.
Headless и оба bench profiles сохраняют свои имена и дают
`hmi-build-profile-mismatch` относительно signed ordinary rev12. Их нельзя
переименовать в manifest или молча выбирать другой профиль.

Даже полностью совпавшие bytes не убирают `unsigned-build-provenance` и
`production-authority-gates-open`. Для paired cohort добавляется
`motion-build-evidence-unavailable`: независимая подпись Motion draft проверена,
но настоящего Motion build artifact/CI provenance/board image нет.

Report `tv4-release-build-review-draft-1` имеет **только** статус
`blocked-build-review-only`, `ci_identity_verified=false`,
`build_provenance_verified=false`, все cohort/outer authority gates false.
Нет ready/success/install/publish пути или параметра override. Report unsigned,
metadata-only, для человека; не installer input/approval token. Config/partition
bytes, actual layout/versions/keys/protocol mapping, signer custody, license,
exact owner approval, migration/recovery/resources/hardware gates ещё открыты.

Current PROJECT_VER остаётся git-derived. Если descriptor не совпадает с signed
component SemVer, прежний verifier отклоняет входы: инструмент не подменяет
версию и не пересобирает app. Fixture records с SemVer — синтетические unsigned
claims, не текущий publishable CI build. Новые actual versions не назначены.

## Команда и exit codes

Собрать прежние host frontend/signer dependencies как в
[cohort review](RELEASE_COHORT_REVIEW.md). Ко всем explicit cohort arguments
добавить четыре local options:

```bash
python3 tools/review_release_build.py \
  --context "$TV4_REVIEWED_LOCAL_CONTEXT" \
  --release-version "$TV4_REVIEWED_RELEASE" --source-commit "$TV4_REVIEWED_SOURCE_SHA" \
  --channel "$TV4_REVIEWED_CHANNEL" --hmi-sdk "$TV4_REVIEWED_SDK" \
  --hmi-manifest "$TV4_HMI_MANIFEST" --hmi-signature "$TV4_HMI_ENVELOPE" \
  --hmi-public-key "$TV4_HMI_PUBLIC_DER" --hmi-image "$TV4_HMI_IMAGE" \
  --hmi-build-candidate "$TV4_REVIEWED_CANDIDATE_DIRECTORY" \
  --hmi-candidate-sha256 "$TV4_EXTERNALLY_REVIEWED_CANDIDATE_SHA256" \
  --hmi-build-profile "$TV4_REVIEWED_BUILD_PROFILE" --hmi-elf "$TV4_HMI_ELF"
```

Optional Motion — прежние пять options, все вместе. Не использовать сохранённый
cohort JSON вместо повторных подписей/full image checks. Вывод только stdout,
никаких файлов отчёта/ключей/образов инструмент не перезаписывает.

| Exit | Значение |
|---|---|
| 3 | Сформирован корректный **blocked** audit JSON, установка/публикация запрещены |
| 1 | Inputs/crypto/policy/pin/bytes отвергнуты, stdout пуст |
| 2 | Ошибка CLI arguments, stdout пуст |

Exit0 для нового audit не предусмотрен. Legacy cohort/build verifier продолжают
свои обычные review exit0; это не разрешение на выпуск или запись.

## Приёмка и CI

Одна новая root CTest suite,13 methods: actual independent signatures с двумя
RAM-only ephemeral P-256 keys, paired/HMI-only, оба SDK×четыре profiles,
pin/source/SDK/schema/scope/layout/image/ELF/signature/peer faults, regular-file
limits/FIFO/symlink, captured snapshots/между-verifier замена и CLI exit semantics.
Старые CLI reports проверены на сохранение формы. Новых private key files нет.

CI сохраняет матрицу8 и точные pins. Все SDK jobs записывают
`candidate-pin.sha256` в metadata-only evidence и проверяют его через existing
build verifier. Это self-generated integrity pin данного job, не независимое
approval/attestation. Host/sanitizers выполняют новую suite; прежние embedded
ARM/source/static/scope/board/output gates сохранены. Firmware C/UI/PROJECT_VER/
config/partitions/dependencies и аппаратные параметры не меняются.

Нет network/device/SWD/UART/reset/Flash/backup rereads, CAN/motion/installer,
release/tag/secrets или самостоятельного merge. Откат — revert PR без операций
на платах; ESP32 остаётся ordinary8f0d899 по последнему owner report.

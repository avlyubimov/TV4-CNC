"""Configuration negatives: timer design may never enter a board image."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--default-arm-build', required=True, type=Path)
    args = parser.parse_args()
    targets = subprocess.check_output(['cmake', '--build', str(args.default_arm_build),
                                      '--target', 'help'], text=True)
    assert 'tv4_stepdir_design' not in targets, 'design leaked into default ARM target'
    toolchain = ROOT / 'firmware/stm32-motion/cmake/arm-none-eabi.cmake'
    with tempfile.TemporaryDirectory() as directory:
        for flag in ['TV4_MOTION_BOARD_IMAGE', 'TV4_MOTION_LINK_PROBE']:
            command = ['cmake', '-S', str(ROOT / 'firmware/stm32-motion'),
                       '-B', str(Path(directory) / flag), '-G', 'Ninja',
                       '-DCMAKE_TOOLCHAIN_FILE=' + str(toolchain),
                       '-DTV4_MOTION_STEPDIR_DESIGN=ON', '-D' + flag + '=ON',
                       '-DTV4_MOTION_BOARD_PROFILE=stm32f407vg-hsi-no-motion-v1']
            result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            assert result.returncode != 0 and 'STEP/DIR design cannot be combined' in result.stdout
    print('PASS default ARM exclusion and both real/synthetic mixed-image refusals; no device')


if __name__ == '__main__':
    main()

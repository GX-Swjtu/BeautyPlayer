"""Static ARM64/16 KB/dependency audit; does not claim device execution."""
import hashlib
import json
import pathlib
import struct
import sys
import zipfile

apk = pathlib.Path(sys.argv[1])
report_path = pathlib.Path(sys.argv[2])
checks = []

def check(name, passed, details=None):
    checks.append(dict(name=name, passed=bool(passed), details=details))

with zipfile.ZipFile(apk) as archive:
    libraries = [entry for entry in archive.infolist() if entry.filename.endswith('.so')]
    check('native_libraries_present', bool(libraries))
    check('only_arm64', all(entry.filename.startswith('lib/arm64-v8a/') for entry in libraries))
    check('no_unused_gpl_modules', not any(any(part in entry.filename.lower() for part in
          ('virtualkeyboard', 'lottie', 'quick3d', 'qmldbg')) for entry in libraries))
    names = {pathlib.PurePosixPath(entry.filename).name for entry in libraries}
    for required in ('libBeautyPlayer_arm64-v8a.so', 'libmars-face-kit.so', 'libc++_shared.so',
                     'libQt6Multimedia_arm64-v8a.so', 'libplugins_multimedia_ffmpegmediaplugin_arm64-v8a.so'):
        check('bundled_' + required, required in names)
    system = {'libc.so', 'libm.so', 'libdl.so', 'liblog.so', 'libandroid.so', 'libEGL.so', 'libGLESv1_CM.so',
              'libGLESv2.so', 'libGLESv3.so', 'libz.so', 'libjnigraphics.so', 'libOpenSLES.so',
              'libmediandk.so', 'libvulkan.so', 'libaaudio.so', 'libcamera2ndk.so'}
    missing = []
    for entry in libraries:
        data = archive.read(entry)
        check(entry.filename + ':elf_arm64', data[:6] == b'\x7fELF\x02\x01' and struct.unpack_from('<H', data, 18)[0] == 183)
        phoff = struct.unpack_from('<Q', data, 32)[0]
        phsize, phcount = struct.unpack_from('<HH', data, 54)
        headers = [struct.unpack_from('<IIQQQQQQ', data, phoff + phsize * i) for i in range(phcount)]
        loads = [p for p in headers if p[0] == 1]
        check(entry.filename + ':16kb_load_segments', all(p[7] >= 16384 and p[2] % 16384 == p[3] % 16384 for p in loads))
        dynamic = next((p for p in headers if p[0] == 2), None)
        if dynamic:
            entries = [struct.unpack_from('<qQ', data, pos) for pos in range(dynamic[2], dynamic[2] + dynamic[5], 16)]
            strings_address = next(value for tag, value in entries if tag == 5)
            strings_segment = next(p for p in loads if p[3] <= strings_address < p[3] + p[5])
            strings = strings_segment[2] + strings_address - strings_segment[3]
            for tag, value in entries:
                if tag == 1:
                    name = data[strings + value:data.index(b'\0', strings + value)].decode('ascii')
                    if name not in names and name not in system:
                        missing.append([entry.filename, name])
    check('all_needed_libraries_resolved', not missing, missing)
report = dict(apk=apk.name, bytes=apk.stat().st_size, sha256=hashlib.sha256(apk.read_bytes()).hexdigest(),
              nativeLibraryCount=len(libraries), passed=all(c['passed'] for c in checks), checks=checks)
report_path.parent.mkdir(parents=True, exist_ok=True)
report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
print(json.dumps({key: report[key] for key in ('apk', 'bytes', 'sha256', 'nativeLibraryCount', 'passed')}))
for item in checks:
    if not item['passed']:
        print('FAIL:', item['name'], item['details'])
sys.exit(0 if report['passed'] else 1)

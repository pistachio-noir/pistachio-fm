project('pistachio-fm', 'cpp', version: '1', default_options: ['cpp_std=c++20', 'buildtype=debugoptimized'])

if meson.get_compiler('cpp').get_id() != 'clang'
  error('Configure with CC=clang CXX=clang++')
endif

qt = import('qt6')
qt_dep = dependency('qt6', modules: ['Core', 'Gui', 'Qml', 'Quick'], version: '>=6.9')
qt_test = dependency('qt6', modules: ['Test'])
python = import('python').find_installation()
inc = include_directories('src')
headers = files('src/FolderModel.h', 'src/FileOps.h', 'src/Places.h', 'src/Apps.h',
                'src/MimeSettings.h', 'src/Preview.h')
moc = qt.preprocess(moc_headers: headers, dependencies: qt_dep)
resources = qt.preprocess(qresources: 'meson/qml.qrc')
backend = static_library('pfm',
  files('src/Format.cpp', 'src/FolderModel.cpp', 'src/FileOps.cpp', 'src/Places.cpp',
        'src/Transfer.cpp', 'src/Apps.cpp', 'src/MimeSettings.cpp', 'src/Preview.cpp',
        'src/qmlregistration.cpp'),
  moc, resources, include_directories: inc, dependencies: qt_dep,
  cpp_args: ['-DPFM_MESON_VERSION="@0@"'.format(meson.version())])
version = custom_target('version', output: 'version.h', build_always_stale: true,
  command: [python, files('meson/version.py'), meson.project_source_root(), '@OUTPUT@', meson.project_version()])
executable('pistachio-fm', 'src/main.cpp', version, include_directories: inc,
  link_whole: backend, dependencies: qt_dep, install: true)
backend_moc = qt.preprocess(moc_sources: 'tests/tst_backend.cpp', dependencies: [qt_dep, qt_test])
tst = executable('tst_backend', 'tests/tst_backend.cpp', backend_moc, include_directories: inc,
  link_whole: backend, dependencies: [qt_dep, qt_test])
test('backend', tst, env: ['QT_QPA_PLATFORM=offscreen'])
executable('pfm-shots', 'tests/shots.cpp', include_directories: inc,
  link_whole: backend, dependencies: [qt_dep, qt_test])

entry = configuration_data()
entry.set('EXECUTABLE', get_option('prefix') / get_option('bindir') / 'pistachio-fm')
configure_file(input: 'data/pistachio-fm.desktop.in', output: 'pistachio-fm.desktop',
  configuration: entry, install_dir: get_option('datadir') / 'applications')
install_data('data/pistachio-fm.svg', install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps')

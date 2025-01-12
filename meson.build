project('libQ', 'cpp', default_options: ['cpp_std=c++17'])

glm_dep = dependency('glm', required: true)
glew_dep = dependency('glew', required: true)

c_sources = files(
  'src/algorithms.c'
)

cpp_sources = files(
  'src/camera.cpp',
  'src/cfifo.cpp',
  'src/cycle_timer.cpp',
  'src/event_timer.cpp',
  'src/log.cpp',
  'src/shader.cpp',
  'src/xml.cpp',
  'src/libQ.cpp'
)

sources = cpp_sources

inc = include_directories('include')

libQ = shared_library(
  'Q',
  sources,
  dependencies: [glm_dep, glew_dep],
  include_directories: inc,
  install: true
)

#libQ.set('version', '1.0.0')
#install_headers('include/*.h', subdir: 'libQ')
#install_subdir('include', install_dir: 'libQ')

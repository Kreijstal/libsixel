sources = [
  'allocator.c',
  'chunk.c',
  'decoder.c',
  'dither.c',
  'encoder.c',
  'frame.c',
  'fromgif.c',
  'frompnm.c',
  'fromsixel.c',
  'loader.c',
  'output.c',
  'pixelformat.c',
  'quant.c',
  'scale.c',
  'status.c',
  'tests.c',
  'tosixel.c',
  'tty.c',
  'writer.c'
]

libsixel = library('sixel',
  sources,
  include_directories: [inc, inc_config],
  dependencies: libsixel_deps,
  version: '1.0.0',
  install: true)

libsixel_dep = declare_dependency(
  link_with: libsixel,
  include_directories: inc,
)

pkgconfig = import('pkgconfig')
pkgconfig.generate(libsixel, name: 'libsixel', description: pkg_description)

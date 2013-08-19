{
  'targets': [
    {
      'target_name': 'hiredis',
      'sources': [
          'src/hiredis.cc'
        , 'src/reader.cc'
      ],
      'include_dirs': ["<!(node -p -e \"require('path').dirname(require.resolve('nan'))\")"],
      'dependencies': [
        'deps/hiredis.gyp:hiredis'
      ],
      'defines': [
          '_GNU_SOURCE'
      ],
      'cflags': [
          '-Wall',
          '-O3'
      ]
    }
  ]
}

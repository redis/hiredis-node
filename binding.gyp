{
  'targets': [
    {
      'target_name': 'hiredis',
      'sources': [
          'src/hiredis.cc'
        , 'src/reader.cc'
      ],
      'dependencies': [
        'deps/hiredis.gyp:hiredis-c'
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

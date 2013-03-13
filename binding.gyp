{
  'targets': [
    {
      'target_name': 'nodehiredis',
      'sources': [
          'hiredis.cc'
        , 'reader.cc'
      ],
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

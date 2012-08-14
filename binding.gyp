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
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-Wall', '-O3'
            ]
          }
        }]
      ]
    }
  ]
}

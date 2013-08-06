{
  'targets': [
    {
      'target_name': 'hiredis-c',
      'type': 'static_library',
      'direct_dependent_settings': {
        'include_dirs': [ '.' ],
      },
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'GCC_C_LANGUAGE_STANDARD': 'c99'
          }
        }],
        ['OS=="solaris"', {
          'cflags+': [ '-std=c99' ]
        }],
        ['OS == "win"', {
          'sources': [
            './hiredis-win/hiredis.c',
            './hiredis-win/net.c',
            './hiredis-win/sds.c',
            './hiredis-win/async.c',
          ]
        }, {
          'sources': [
            './hiredis/hiredis.c',
            './hiredis/net.c',
            './hiredis/sds.c',
            './hiredis/async.c',
          ]
        }],
      ]
    }
  ]
}

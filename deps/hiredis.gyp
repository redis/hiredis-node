{
  'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      'Debug': {
        'defines': [ 'DEBUG', '_DEBUG' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 1, # static debug
          },
        },
      },
      'Release': {
        'defines': [ 'NDEBUG' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 0, # static release
          },
        },
      }
    },
    'msvs_settings': {
      'VCCLCompilerTool': {
      },
      'VCLibrarianTool': {
      },
      'VCLinkerTool': {
        'GenerateDebugInformation': 'true',
      },
    },
    'conditions': [
      ['OS == "win"', {
        'defines': [
          'WIN32'
        ],
      }]
    ],
  },

  'targets': [
    {
      'target_name': 'hiredis',
      'type': 'static_library',
      #'include_dirs': [ '.' ],
      'direct_dependent_settings': {
        'include_dirs': [ '.' ],
      },
      'defines': [
      ],
      'sources': [
        './hiredis/hiredis.c',
        './hiredis/net.c',
        './hiredis/sds.c',
        './hiredis/async.c',
      ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'GCC_C_LANGUAGE_STANDARD': 'c99'
          }
        }]
      ]
    },

    {
      'target_name': 'test',
      'type': 'executable',
      'dependencies': [ 'hiredis' ],
      'sources': [ './hiredis/test.c' ]
    }
  ]
}

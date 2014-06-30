{
  'targets': [
    {
      'target_name': 'pycrosswalk',
      'type': 'loadable_module',
      'include_dirs': [
        '.',
      ],
      'sources': [
        'src/pycrosswalk.c',
        'xwalk/XW_Extension.h',
        'xwalk/XW_Extension_Runtime.h',
        'xwalk/XW_Extension_SyncMessage.h',
      ],
      'cflags': [
        '<!@(pkg-config --cflags python-<(python_version))',
        '-g',
        '-fPIC',
      ],
      'link_settings': {
        'ldflags': [
          '<!@(pkg-config --libs-only-L --libs-only-other python-<(python_version))',
          '-g',
        ],
        'libraries': [
          '<!@(pkg-config --libs-only-l python-<(python_version)) -lcallback',
        ],
      },
    },
  ],
}

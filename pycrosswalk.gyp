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
        'xwalk/XW_Extension_SyncMessage.h',
      ],
      'cflags': [
        '<!@(pkg-config --cflags python-3.3)',
        '-fPIC',
      ],
      'link_settings': {
        'ldflags': [
          '<!@(pkg-config --libs-only-L --libs-only-other python-3.3)',
        ],
        'libraries': [
          '<!@(pkg-config --libs-only-l python-3.3)',
        ],
      },
    },
  ],
}

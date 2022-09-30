if not ssl.found()
  subdir_done()
endif

# FIXME: prevent install during main install, but not during test :/
ssl_passphrase_callback = shared_module('ssl_passphrase_func',
  ['ssl_passphrase_func.c'],
  kwargs: pg_mod_args + {
    'dependencies': [ssl, pg_mod_args['dependencies']],
  },
)
testprep_targets += ssl_passphrase_callback

# Targets to generate or remove the ssl certificate and key. Need to be copied
# to the source afterwards. Normally not needed.

openssl = find_program('openssl', native: true, required: false)

if openssl.found()
  cert = custom_target('server.crt',
    output: ['server.crt', 'server.ckey'],
    command: [openssl, 'req', '-new', '-x509', '-days', '10000', '-nodes', '-out', '@OUTPUT0@',
      '-keyout', '@OUTPUT1@', '-subj', '/CN=localhost'],
    build_by_default: false,
    install: false,
  )

  # needs to agree with what's in the test script
  pass = 'FooBaR1'

  custom_target('server.key',
    input: [cert[1]],
    output: ['server.key'],
    command: [openssl, 'rsa', '-aes256', '-in', '@INPUT0@', '-out', '@OUTPUT0@', '-passout', 'pass:@0@'.format(pass)]
  )
endif

tests += {
  'name': 'ssl_passphrase_callback',
  'sd': meson.current_source_dir(),
  'bd': meson.current_build_dir(),
  'tap': {
    'tests': [
      't/001_testfunc.pl',
    ],
    'env': {'with_ssl': 'openssl'},
  },
}
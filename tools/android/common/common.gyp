# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'android_tools_common',
      'type': '<(library)',
      'include_dirs': [
        '..',
        '../../..',
      ],
      'sources': [
        'adb_connection.cc',
        'daemon.cc',
        'net.cc',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:

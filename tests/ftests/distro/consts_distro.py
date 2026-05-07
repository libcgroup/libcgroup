# SPDX-License-Identifier: LGPL-2.1-only
#
# Distro resolver for the libcgroup functional tests constants
#
# Copyright (c) 2026 Oracle and/or its affiliates.
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>
#

from .consts_exceptions import UnsupportedDistroError
from .consts_exceptions import OsReleaseError


class DistroConstsSelector:
    """Resolve distro-specific expected-output profiles."""

    def __init__(self, supported_distros, default_distro='ubuntu', id_map=None):
        # Normalize distro keys once to avoid repeated case-folding at lookup.
        self._supported_distros = {
            distro.lower(): profile for distro, profile in supported_distros.items()
        }

        self._default_distro = default_distro.lower()
        self._id_map = {
            distro: distro for distro in self._supported_distros.keys()
        }

        # Apply explicit OS-release ID mappings after the default self-map:
        # this normalizes short/vendor IDs (e.g. "ol") to a supported distro
        # key and intentionally overrides any same-key default mapping.
        if id_map:
            for distro_id, distro in id_map.items():
                self._id_map[distro_id.lower()] = distro.lower()

    @property
    def default_distro(self):
        return self._default_distro

    def resolve(self, config=None):
        """Return a distro object and distro key."""
        os_release = self._read_os_release()
        distro_id = os_release.get('ID', '').strip().strip('"').lower()

        if not distro_id:
            raise OsReleaseError('/etc/os-release', 'missing required ID field')

        distro = self._id_map.get(distro_id)
        if distro and distro in self._supported_distros:
            return self._supported_distros[distro], distro

        raise UnsupportedDistroError('/etc/os-release:ID',
                                     distro_id,
                                     sorted(self._supported_distros.keys()))

    @staticmethod
    def _read_os_release():
        release_file = '/etc/os-release'
        release_data = {}

        try:
            with open(release_file, 'r') as relfile:
                for line in relfile:
                    line = line.strip()
                    if not line or line.startswith('#'):
                        continue

                    if '=' not in line:
                        continue

                    key, value = line.split('=', 1)
                    key = key.strip()
                    if not key:
                        continue
                    release_data[key] = value.strip().strip('"')
        except OSError as error:
            raise OsReleaseError(release_file, str(error))

        return release_data

# vim: set et ts=4 sw=4:

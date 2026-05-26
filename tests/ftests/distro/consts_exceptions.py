# SPDX-License-Identifier: LGPL-2.1-only
#
# Exceptions used by libcgroup functional test distro constants resolution
#
# Copyright (c) 2026 Oracle and/or its affiliates.
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>
#

class DistroResolutionError(Exception):
    """Base class for distro-constant resolution failures."""


class UnsupportedDistroError(DistroResolutionError):
    """Raised when /etc/os-release ID is not supported."""

    def __init__(self, source, distro_id, supported_distros):
        self.source = source
        self.distro_id = distro_id
        self.supported_distros = tuple(supported_distros)

        message = ('Unsupported distro from {}: "{}" (supported: {})').format(
                        self.source, self.distro_id, ', '.join(self.supported_distros)
                  )
        super(UnsupportedDistroError, self).__init__(message)


class OsReleaseError(DistroResolutionError):
    """Raised when /etc/os-release cannot be parsed for distro detection."""

    def __init__(self, source, reason):
        self.source = source
        self.reason = reason

        message = 'Unable to resolve distro from {}: {}'.format(
            self.source, self.reason
        )
        super(OsReleaseError, self).__init__(message)

# vim: set et ts=4 sw=4:

#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from schema import RegistrationMode


REGISTRATION_MODES = (
    RegistrationMode("runtime"),
    RegistrationMode("static"),
    RegistrationMode("mixed"),
)

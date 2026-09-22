# SPDX-License-Identifier: GPL-2.0-only
# Derived from exlaunch f9f4b0dd07b68f97958cb9c79228bbca22ca80d5.
# Set the Silksong program ID, Python command and NPDM template.
# See THIRD_PARTY_NOTICES.md and licenses/source-inventory.json.
#----------------------------- User configuration -----------------------------

# Common settings
#------------------------

# This project builds a Module (subsdk9) for the target game.
LOAD_KIND := Module

# Program you're targetting. Used to determine where to deploy your files.
PROGRAM_ID := 010013C00E930000

# Optional path to copy the final ELF to, for convenience.
ELF_EXTRACT :=

# Python command to use. Must be Python 3.4+.
PYTHON := python

# Additional C/C++ flags to use.
C_FLAGS := 
CXX_FLAGS := 

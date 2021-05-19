#!/usr/bin/env python3

"""
Reads RESULT lines from stdin and writes equivalent TSV to stdout.
"""

import sys
from textwrap import dedent

def flush_record(r):
    print('\t'.join(r))

# Checks if arguments are in a prefix relation
def are_keys_compatible(ks1, ks2):
    return all(k1 == k2 for k1,k2 in zip(ks1, ks2))

global_keys = None

for line in map(str.rstrip, sys.stdin):
    first_word, *following_words = line.split()

    if first_word != 'RESULT': continue

    keys, values = zip(*(w.split('=') for w in following_words))

    # Output keys once, assert all following are identical
    if global_keys:
        if not are_keys_compatible(global_keys, keys):
            raise RuntimeError(dedent(f"""\
                Expected keys to be {global_keys} but they were {keys}
                when reading line '{line}'"""))
    else:
        global_keys = keys
        flush_record(keys)

    flush_record(values)

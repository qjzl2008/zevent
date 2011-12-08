import os
import sys

from os.path import abspath
from os.path import isfile

import xreload

MOD_TIMES = dict()


def in_search_path(path):
    if 'logics' in path:
        return True
    else:
	return False

def search_modules():
    modules = []
    for name, module in sys.modules.items():
        if module is not None:
            f = getattr(module, '__file__', None)
            # Standard library modules don't have a __file__
            if f is None:
                continue
            f = abspath(f)
            if in_search_path(f):
                    modules.append((f, module))
    return modules


def get_mod_time(path):
    mtime = 0
    # If we have the compiled source, look for the source code change date
    if path.endswith('pyc') or path.endswith('pyo'):
        source = path[:-1]
        if os.path.isfile(source):
            path = source
    # protect against missing and unaccessible files
    if isfile(path):
        mtime = os.stat(path)[8]
    return mtime


def get_mod_times(event=None):
    global MOD_TIMES
    for path, module in search_modules():
        if path not in MOD_TIMES:
            MOD_TIMES[path] = (get_mod_time(path), module)
    return MOD_TIMES


def check_mod_times():
    changed = []
    for path, (time, module) in get_mod_times().items():
        newtime = get_mod_time(path)
        if time < newtime:
            changed.append((path, newtime, module))
    return changed


def reload_code():
    global MOD_TIMES
    reloaded = []
    for path, time, module in check_mod_times():
        module = xreload.xreload(module)
        MOD_TIMES[path] = (time, module)
        reloaded.append(path)
    return reloaded



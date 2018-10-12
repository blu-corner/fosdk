#!/usr/bin/env python

import multiprocessing
import subprocess
import platform
import json
import re
import os


class PlatformInfo(dict):

    def __init__(self):
	dict.__init__(self)
	mem_bytes = os.sysconf('SC_PAGE_SIZE') * os.sysconf('SC_PHYS_PAGES')
	mem_gib = mem_bytes/(1024.**3)
	distro = platform.linux_distribution()
	distro_string = '-'.join(list(distro))
	cpu_model = self.get_platform().strip()
	system, node, release, version, machine, processor = platform.uname()
	self.update({
	'arch': platform.architecture()[0],
	'system': system,
	'node': node,
	'release': release,
	'version': version,
	'machine': machine,
	'processor': processor,
	'distro': distro_string,
	'cpu_model': cpu_model,
	'cores': multiprocessing.cpu_count(),
	'memory': "{0}.3f GB".format(mem_gib)
	})

    def get_platform(self):
	if platform.system() == "Windows":
	    return platform.processor()

	elif platform.system() == "Darwin":
	    os.environ['PATH'] = os.environ['PATH'] + os.pathsep + '/usr/sbin'
	    return subprocess.check_output(['sysctl', '-n machdep.cpu.brand_string'])

	elif platform.system() == "Linux":
    	    all_info = subprocess.check_output(['cat', '/proc/cpuinfo'])
    	    for line in all_info.split("\n"):
    		if "model name" in line:
	    	    return re.sub( ".*model name.*:", "", line,1)

    def save_as_json(self):
	with open("systemInfo.json", 'w') as meta:
	    print >>meta, json.dumps(self, sort_keys=True, indent=4, separators=(',', ': '))

    def load_as_json(self, jsonFile):
    	with open(jsonFile, 'rb') as JsonFile:
	    decoded = json.load(JsonFile)
            self.update(decoded)

if __name__ == "__main__":
    work = PlatformInfo()
    work.save_as_json()
    work.load_as_json("systemInfo.json")

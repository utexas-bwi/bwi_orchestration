#!/bin/env python

import sys
import yaml

map_file_str = sys.argv[1]
doors_file_str = sys.argv[2]
locations_file_str = sys.argv[3]
x = float(sys.argv[4])
y = float(sys.argv[5])

doors_file=open(doors_file_str, "r")
doors = yaml.load(doors_file)
doors_file.close()

for door in doors:


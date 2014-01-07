#!/usr/bin/env python

from bwi_tools import loadMapFromFile, saveMapToFile
from nav_msgs.msg import OccupancyGrid

import copy
import math
import sys
import yaml

map_file_str = sys.argv[1]
doors_file_str = sys.argv[2]
locations_file_str = sys.argv[3]
x = float(sys.argv[4])
y = float(sys.argv[5])

def normalize_angle(angle):
    return math.atan2(math.sin(angle), math.cos(angle))

def flip_angle_along_y_axis(angle):
    return math.atan2(-math.sin(angle), math.cos(angle))

def flip_angle_along_x_axis(angle):
    return math.atan2(math.sin(angle), -math.cos(angle))

def flip_angle_along_x_and_y_axes(angle):
    return flip_angle_along_x_axis(flip_angle_along_y_axis(angle))

def construct_mirror_point(index, point):
    if index == 1:
        return copy.deepcopy(point)
    if index == 2:
        return [2 * x - point[0], point[1], flip_angle_along_y_axis(point[2])]
    if index == 3:
        return [2 * x - point[0], 2 * y - point[1], 
                flip_angle_along_x_and_y_axes(point[2])]
    
    return [point[0], 2 * y - point[1], flip_angle_along_x_axis(point[2])]

def construct_mirror_door(index, door):
    new_door = {}
    new_door['name'] = str(index) + door['name']
    approach_0 = {}
    approach_0['from'] = door['approach'][0]['from'] \
            if door['approach'][0]['from'] == "cor" \
            else str(index) + door['approach'][0]['from']
    approach_0['point'] = construct_mirror_point(index, 
                                                 door['approach'][0]['point'])
    approach_1 = {}
    approach_1['from'] = door['approach'][1]['from'] \
            if door['approach'][1]['from'] == "cor" \
            else str(index) + door['approach'][1]['from']
    approach_1['point'] = construct_mirror_point(index, 
                                                 door['approach'][1]['point'])
    new_door['approach'] = [approach_0, approach_1]
    new_door['width'] = door['width']
    return new_door

def get_id(location, locations):
    for i, loc in enumerate(locations):
        if loc == location:
            return i

# Handle map
map = loadMapFromFile(map_file_str)
extra_x = map.map.info.origin.position.x - x
extra_y = map.map.info.origin.position.y - y
extra_pixels_x = int(extra_x / map.map.info.resolution)
extra_pixels_y = int(extra_y / map.map.info.resolution)

new_map = OccupancyGrid()
new_map.info.origin = map.map.info.origin
new_map.info.resolution = map.map.info.resolution
new_map.info.width = 2 * (map.map.info.width + extra_pixels_x)
new_map.info.height = 2 * (map.map.info.height + extra_pixels_y)
new_map.data = [None] * new_map.info.height * new_map.info.width

new_map.info.origin.position.x -= (new_map.info.width / 2) * new_map.info.resolution + extra_x
new_map.info.origin.position.y -= (new_map.info.height / 2) * new_map.info.resolution + extra_y
new_map.info.origin = map.map.info.origin

for j in range(new_map.info.height / 2):
    for i in range(new_map.info.width / 2):
        if i < extra_pixels_x or j < extra_pixels_y:
            old_map_value = 0
        else:
            old_map_idx = map.map.info.width * (j - extra_pixels_y) + \
                    (i - extra_pixels_x)
            old_map_value = map.map.data[old_map_idx]
        map_idx_1 = new_map.info.width * (new_map.info.height / 2 + j) + \
                new_map.info.width / 2 + i
        map_idx_2 = new_map.info.width * (new_map.info.height / 2 + j) + \
                new_map.info.width / 2 - i - 1
        map_idx_3 = new_map.info.width * (new_map.info.height / 2 - j - 1) + \
                new_map.info.width / 2 - i - 1
        map_idx_4 = new_map.info.width * (new_map.info.height / 2 - j - 1) + \
                new_map.info.width / 2 + i

        new_map.data[map_idx_1] = new_map.data[map_idx_2] = \
                new_map.data[map_idx_3] = new_map.data[map_idx_4] = \
                old_map_value 

saveMapToFile(new_map, "map.yaml", "map.pgm", False, 0.196, 0.65)

# Handle doors file
doors_file = open(doors_file_str, "r")
doors = yaml.load(doors_file)
doors_file.close()

new_doors_file = open("doors.yaml", "w")
new_doors = []

for door in doors:
    new_doors.append(construct_mirror_door(1, door))
    new_doors.append(construct_mirror_door(2, door))
    new_doors.append(construct_mirror_door(3, door))
    new_doors.append(construct_mirror_door(4, door))

new_doors_file.write(yaml.dump(new_doors))

# Handle locations file
locations_file = open(locations_file_str, "r")
locations = yaml.load(locations_file)
locations_file.close()

new_locations = {}
new_locations['locations'] = []
new_locations['locations'].append("cor")

for location in locations['locations']:
    if location != 'cor':
        new_locations['locations'].append("1" + location)
        new_locations['locations'].append("2" + location)
        new_locations['locations'].append("3" + location)
        new_locations['locations'].append("4" + location)

new_locations['data'] = [get_id("cor", new_locations['locations'])] * \
        new_map.info.height * new_map.info.width
old_cor_idx = get_id("cor", locations['locations'])
for j in range(new_map.info.height / 2):
    for i in range(new_map.info.width / 2):

        if i < extra_pixels_x or j < extra_pixels_y:
            location_idx1 = location_idx2 = location_idx3 = location_idx4 = \
                    get_id("cor", new_locations['locations'])
        else:
            old_map_idx = map.map.info.width * (j - extra_pixels_y) + \
                    (i - extra_pixels_x)
            old_location_idx = locations['data'][old_map_idx]
            if old_location_idx == -1 or old_location_idx == old_cor_idx:
                location_idx1 = location_idx2 = location_idx3 = location_idx4 = \
                        get_id("cor", new_locations['locations'])
            else:
                old_location = locations['locations'][old_location_idx]
                location_idx1 = get_id("1" + old_location, 
                                       new_locations['locations'])
                location_idx2 = get_id("2" + old_location, 
                                       new_locations['locations'])
                location_idx3 = get_id("3" + old_location, 
                                       new_locations['locations'])
                location_idx4 = get_id("4" + old_location, 
                                       new_locations['locations'])
                if location_idx1 == None:
                    print old_location, old_location_idx, location_idx1
        map_idx_1 = new_map.info.width * (new_map.info.height / 2 + j) + \
                new_map.info.width / 2 + i
        new_locations['data'][map_idx_1] = location_idx1
        map_idx_2 = new_map.info.width * (new_map.info.height / 2 + j) + \
                new_map.info.width / 2 - i - 1
        new_locations['data'][map_idx_2] = location_idx2
        map_idx_3 = new_map.info.width * (new_map.info.height / 2 - j - 1) + \
                new_map.info.width / 2 - i - 1
        new_locations['data'][map_idx_3] = location_idx3
        map_idx_4 = new_map.info.width * (new_map.info.height / 2 - j - 1) + \
                new_map.info.width / 2 + i
        new_locations['data'][map_idx_4] = location_idx4
        
new_locations_file = open("locations.yaml", "w")
new_locations_file.write(yaml.dump(new_locations))
